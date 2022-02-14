/*!==========================================================================
* \file
* - Program:       gtest-csummerize
* - File:          main.cpp
* - Created:       04/29/2021
* - Author:        Vitaly Bulganin
* - Description:
* - Comments:
*
-----------------------------------------------------------------------------
*
* - History:
*
===========================================================================*/
#include <gtest/gtest.h>
//-------------------------------------------------------------------------//
#include "units/gtest-pregex.h"
//-------------------------------------------------------------------------//
#include <cgo_api.h>
//-------------------------------------------------------------------------//
RankTermsResult go_callback_rank_terms(uintptr_t h, RankTermsParams) {
    return RankTermsResult();
}
//-------------------------------------------------------------------------//
int main(int argc, char ** argv) {
    // This is a UTF-8 file.
    std::locale::global(std::locale("en_US.UTF-8"));
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
