/*!==========================================================================
* \file
* - Program:       csummerize
* - File:          cgo_api.h
* - Created:       10/09/2021
* - Author:
* - Description:   C wrapper for cgo.
* - Comments:
*
-----------------------------------------------------------------------------
*
* - History:
*
===========================================================================*/
#pragma once
//-------------------------------------------------------------------------//
#ifndef __CGO_API_H_CFE4B0D4_FE66_4653_B4FD_A1FB94633D6A__
#define __CGO_API_H_CFE4B0D4_FE66_4653_B4FD_A1FB94633D6A__
//-------------------------------------------------------------------------//
#ifdef __cplusplus
extern "C" {
#endif
//-------------------------------------------------------------------------//
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
//-------------------------------------------------------------------------//
// указатель на C++ класс Annotator
typedef void * AnnotatorPtr;
// указатель на AnnotateResult
typedef void * AnnotateResultPtr;

typedef struct String {
    char * data;
    size_t size;
} String;

typedef struct StringArray {
    String * data;
    size_t size;
} StringArray;

typedef struct TextAttribute {
    String source;
} TextAttribute;

typedef struct TextFragment {
    String text;
    TextAttribute * attributes;
    size_t attributesSize;
} TextFragment;

typedef struct AnnotateOptions {
    size_t maxSentences;  // список текстов для аннотации
    size_t maxCharLength; // максимальное количество букв текста в выжимках
    float compressRatio; // максимальный процент букв текста в выжимках по
    // сравнению с оригинальным текстом
} AnnotateOptions;

typedef struct AnnotateParams {
    AnnotatorPtr aPtr;
    uintptr_t handle; // callback функция для вызова Go
    String model;
    TextFragment * textFragments;
    size_t textFragmentsSize;
    AnnotateOptions options;
} AnnotateParams;

typedef struct AnnotateResult {
    // указатель на AnnotateResult, который аллоцировал данную структуру
    AnnotateResultPtr self;
    String model; // modified model
    int modelChanged;
    StringArray abstracts; // выжимки текста
    // ключевые слова и словосочетания - unigrams, bigrams,...
    StringArray keywords;
    String error; // сообщение об ошибке
} AnnotateResult;

typedef struct RankTermsParams {
    StringArray terms; // термины для ранжирования
} RankTermsParams;

typedef struct RankTermsResult {
    double * data;    // ранги терминов
    size_t size;      // количество рангов
    size_t totalDocuments; // общее количество документов в системе
    String error;     // сообщение об ошибке
} RankTermsResult;

typedef struct AnnotatorInitParams {
    String json; // json строка, содержащая настройки конфига
} AnnotatorInitParams;

typedef struct AnnotatorInitResult {
    AnnotatorPtr ptr;
    char * error;
} AnnotatorInitResult;

// загружает конфиг аннотатора из файла
AnnotatorInitResult AnnotatorInit(AnnotatorInitParams);

// Аннотирует текст на основании модели и возвращает обновленную модель с
// выжимками текстов
AnnotateResult Annotate(AnnotateParams);

// освобождает память аннотатора
void AnnotatorFree(AnnotatorPtr);
// освобождает память результата аннотации
void AnnotateResultFree(AnnotateResultPtr);

extern RankTermsResult go_callback_rank_terms(uintptr_t h, RankTermsParams);
//-------------------------------------------------------------------------//
#ifdef __cplusplus
}
#endif
//-------------------------------------------------------------------------//
#endif // __CGO_API_H_CFE4B0D4_FE66_4653_B4FD_A1FB94633D6A__
