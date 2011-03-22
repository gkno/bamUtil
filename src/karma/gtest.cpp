/*
 * Copyright (c) 2009 Regents of the University of Michigan
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "GenomeSequence.h"
#include "WordIndex.h"

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <gtest/gtest.h>

class GlobalTestEnvironment : public ::testing::Environment
{
public:
    virtual ~GlobalTestEnvironment();
    virtual void SetUp();
    virtual void TearDown();
public:
    GenomeSequence gs;
    GenomeSequence csgs;
    WordIndex wi;
    WordIndex cswi;
};

GlobalTestEnvironment::~GlobalTestEnvironment()
{
}

void GlobalTestEnvironment::SetUp()
{
//     if ( gs.open(false) || csgs.open(true) ||
//          wi.open(gs) || cswi.open(csgs))
//     {
//         printf("Open genome sequence or word index file failed!! \n");
//         exit(1);
//     }

}

void GlobalTestEnvironment::TearDown()
{
}


::testing::Environment* const globalEnvironment = ::testing::AddGlobalTestEnvironment(new GlobalTestEnvironment);

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
