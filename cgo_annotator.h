// нужен для интерфейса к cgo
//-------------------------------------------------------------------------//
#ifndef _CGO_ANNOTATOR_HPP_D28CC1D7_DAD8_4A37_83DD_E6ACFB279C5E__
#define _CGO_ANNOTATOR_HPP_D28CC1D7_DAD8_4A37_83DD_E6ACFB279C5E__
//-------------------------------------------------------------------------//
#include <cstdint>
#include <string>
//-------------------------------------------------------------------------//
#include <mtc/config.h>
//-------------------------------------------------------------------------//
#include "cgo_api.h"
#include "config.h"
#include "domain.h"
//-------------------------------------------------------------------------//
namespace cgo {
//-------------------------------------------------------------------------//
    class Annotator {
        //!< Keeps configuration.
        csummerize::config config;

    public:
        /**
         * Constructor.
         * @param params [in] - A set of parameters.
         */
        Annotator(const AnnotatorInitParams & params);

        /**
         * Annotates a text.
         * @param params [in] - A annotation parameters.
         * @param res [out] - Annotated result.
         */
        auto Annotate(const AnnotateParams & params, AnnotateResult * res) const -> void;
    };
//-------------------------------------------------------------------------//
} // namespace cgo
//-------------------------------------------------------------------------//
#endif // _CGO_ANNOTATOR_HPP_D28CC1D7_DAD8_4A37_83DD_E6ACFB279C5E__
