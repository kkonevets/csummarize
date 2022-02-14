#include "../../cgo_annotator.h"
//-------------------------------------------------------------------------//
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>
#include <functional>
//-------------------------------------------------------------------------//
#include <moonycode/codes.h>
//-------------------------------------------------------------------------//
#include "../../cgo_tools.h"
//-------------------------------------------------------------------------//
#include "../../profiler.h"
//-------------------------------------------------------------------------//
namespace cgo {
//-------------------------------------------------------------------------//
    namespace {
        // Validates input arguments.
        auto validate(const csummerize::config & config, const AnnotateParams & params, AnnotateResult * result) -> void {
            if (not result) {
                throw (std::invalid_argument("Invalid pointer on annotated result"));
            }
            if (params.model.data && params.model.size == 0) {
                throw (std::invalid_argument("Serialized model incorrect"));
            }
            if (config.get_zmap().empty()) {
                throw (std::invalid_argument("Annotator's configuration empty"));
            }
        }
        // Wrapper, which helps to avoid memory leads.
        template<typename object_t>
        struct object_guard final {
            //!< Keeps object.
            object_t object;
            //!< Keeps a deleter.
            std::function<void(object_t & obj)> ondeleter;

            /**
             * Constructor.
             * @param deleter [in] - A deleter function.
             */
            object_guard(std::function<void(object_t & obj)> deleter) : object(object_t()), ondeleter(deleter) {
            }

            /**
             * Constructor.
             * @param obj [in] - An object.
             * @param deleter [in] - A deleter function.
             */
            object_guard(const object_t & obj, std::function<void(object_t & obj)> deleter) : object(obj), ondeleter(deleter) {
            }

            /**
             * Destructor.
             * @throw none.
             */
            ~object_guard() noexcept {
                try {
                    if (this->ondeleter) {
                        this->ondeleter(this->object);
                    }
                } catch (const std::exception & exc) {
                    fprintf(stderr, "[error] %s\n", exc.what());
                }
            }

            //!< Gets a reference on object.
            operator object_t &() noexcept {
                return this->object;
            }
        };
    }; // namespace
//-------------------------------------------------------------------------//
    Annotator::Annotator(const AnnotatorInitParams & params)
      : config(std::move(csummerize::build_config(mtc::config::Load(params.json.data, nullptr)))) {
    }

    void Annotator::Annotate(const AnnotateParams & params, AnnotateResult * res) const {
#if defined(PROFILE_ENABLED)
        csummerize::profiler profiler([](std::uint64_t elapsed) {
            std::clog << "[CGO] [Annotate] - " << elapsed/1000.0 << " sec" << std::endl;
        });
#endif // defined(PROFILE_ENABLED)
        try {
            res->error = StringNew(); // dropping error on.
            // Setting a new state (unchanged).
            res->modelChanged = 0;
            if (not params.textFragments || (params.textFragments && params.textFragmentsSize == 0)) {
                throw (std::invalid_argument("No text found to annotate"));
            }
            // Validating input arguments.
            validate(this->config, params, res);
            // Getting a max count of sentences.
            const auto max_sentences_count = (params.options.maxSentences > 0) ? params.options.maxSentences : this->config.get_zmap().get_int32("sentences", 5);
            // Getting a max count of chars in abstraction.
            const auto max_chars_count = params.options.maxCharLength;
            // Creating a domain.
            auto domain = csummerize::build_domain(this->config);
            // Deserializing a domain from serialized model.
            if (params.model.data && not domain->FetchFrom(mtc::sourcebuf(params.model.data, params.model.size).getptr())) {
                throw (std::invalid_argument("Deserialize model failed"));
            }
            // Keeps a list of ranks from outsize.
            std::unique_ptr<object_guard<RankTermsResult>> ranks;
            // Creating a list of terms.
            object_guard<RankTermsParams> term_params([](RankTermsParams & params) -> void {
                if (params.terms.data) {
                    StringArrayFree(params.terms);
                }
            });
            std::uint32_t length = 0;
            // Adding a list of texts.
            for (auto i = 0; i < params.textFragmentsSize; ++i) {
                const auto & fragment = params.textFragments[i];
                if (fragment.text.data && fragment.text.size > 0) {
                    // Getting a text.
                    const auto text = std::move(std::string(fragment.text.data, fragment.text.size));
                    // Accumulating a length of text.
                    length += text.length();
                    // Adding a new text to domain.
                    domain->add(text);
                }
            }
            // Calculating a compression value of text.
            const auto compression = (params.options.compressRatio > 0.0) ? length*params.options.compressRatio : 0.0;
            if (params.handle) {
                // Allocating memory on keeping a list of terms.
                const auto terms = domain->get_terms();
                if (not terms.empty()) {// Trying to get a list of rank terms.
                    // Allocating a memory.
                    term_params.object.terms = StringArrayNew(terms.size());
                    for (auto i = 0; i < terms.size(); ++i) {
                        // Setting a term.
                        term_params.object.terms.data[i] = StringNew(terms[i].data(), terms[i].size());
                        assert(term_params.object.terms.data[i].data && term_params.object.terms.data[i].size > 0 && "Invalid term value");
                    }
                    // Getting a list of terms from outsize.
                    ranks = std::move(std::make_unique<object_guard<RankTermsResult>>(go_callback_rank_terms(params.handle, term_params), [](RankTermsResult & result) {
                        if (result.data) {
                            free(result.data);
                            result = {.data = nullptr, .size = 0};
                        }
                        if (result.error.data) {
                            StringFree(result.error);
                        }
                    }));
                }
            }
            // Getting annotated list.
            const auto annotated = domain->abstract(max_sentences_count, [&](const csummerize::lexeme & lexeme, std::int64_t total_doc_count, double rank) -> double {
/*
            T1 - количество документов в первом корпусе (моя статистика), Т2 - во втором (письма)
            w1 - вес термина в первом корпусе, w2 - во втором
            положим k=T2/T1 (k от 0 до 1)
            тогда новый вес будет (1-k)*w1 + k*w2
*/
                if (ranks) {// Applying ranks.
                    if (ranks->object.error.data && ranks->object.error.size > 0) {
                        std::fprintf(stderr, "[error] Get a list of ranks from outside failed: %s", std::string(ranks->object.error.data, ranks->object.error.size).c_str());
                    } else {
                        // Keeps a count of total letters.
                        std::int64_t total_letters_count = ranks->object.totalDocuments;
                        for (auto i = 0; i < ranks->object.size; ++i) {
                            // Расчитываем коэффициент сжатия двух статистик.
                            const auto k = static_cast<double>(total_letters_count)/static_cast<double>(total_doc_count);
                            // Calculating a new rank of term.
                            rank = (1.0 - k)*rank + k*ranks->object.data[i];
                        }
                    }
                }
                return rank;
            });
            std::vector<std::string> buffer;
            for (auto i = 0, len = 0; i < annotated.size(); ++i) {
                if (not annotated[i]->get_preview()->get_text().empty()) {
                    auto wide = annotated[i]->get_preview()->get_text();
                    const auto length = wide.length();
                    // Calculating a length of text.
                    len += length;
                    if (compression > 0.0 && len > compression) {
                        for (int j = length - 1; j >= 0; --j) {
                            if (wide[j] == widechar(L'\u0020') && (len - length + j) < (params.options.maxCharLength - 1)) {
                                wide = wide.substr(0, j) + widechar(L'\u2026');
                                // Setting a new length of texts.
                                len -= length - j;
                                break;
                            }
                        }
                    }
                    if (params.options.maxCharLength > 0 && params.options.maxCharLength < (UINT32_MAX - 1) && len >= params.options.maxCharLength) {
                        for (int j = length - 1; j >= 0; --j) {
                            if (wide[j] == widechar(L'\u0020') && (len - length + j) < (params.options.maxCharLength - 1)) {
                                wide = wide.substr(0, j) + widechar(L'\u2026');
                                // Finishing a parent loop.
                                i = annotated.size() + 1;
                                break;
                            }
                        }
                        if (i <= annotated.size()) {
                            break;
                        }
                    }
                    // Saving a text in utf8.
                    buffer.emplace_back(codepages::widetombcs(codepages::codepage_utf8, wide.data(), wide.size()));
                }
            }
            // Allocating a buffer.
            res->abstracts = StringArrayNew(buffer.size());
            for (auto i = 0; i < buffer.size(); ++i) {
                // Saving a text in utf8.
                res->abstracts.data[i] = StringNew(buffer[i].c_str());
            }
            // Getting a list of idioms.
            res->keywords = StringArray{.data = nullptr, .size = 0};
            auto idioms = domain->get_idioms();
            if (not idioms.empty()) {// Copying a list of idioms.
                res->keywords = StringArrayNew(idioms.size());
                for (auto i = 0; i < idioms.size(); ++i) {
                    res->keywords.data[i] = StringNew(idioms[i].c_str());
                }
            }
            // Allocating memory to serialize model.
            res->model = StringNew(domain->GetBufLen());
            // Serializing current model.
            domain->Serialize(res->model.data);
            // Setting a new state (changed).
            res->modelChanged = 1;
        } catch (const std::exception & exc) {
            res->error.data = strdup(exc.what());
            res->error.size = std::strlen(exc.what());
        }
    }
//-------------------------------------------------------------------------//
}; // namespace cgo
