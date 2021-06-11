/******************************************************************************
 *
 * Project:  Mapserver
 * Purpose:  Fuzzer
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2021, Even Rouault <even.rouault at spatialys.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include <unistd.h>

#include "cpl_conv.h"
#include "mapserver.h"
extern "C"
{
#include "mapserv.h"
}

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv);
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len);

msIOContext stdin_ctx;
msIOContext stdout_ctx;
msIOContext stderr_ctx;

static int dummyRead( void *context, void *data, int byteCount )
{
    (void)context;
    (void)data;
    (void)byteCount;
    return 0;
}


static int dummyWrite( void *context, void *data, int byteCount )
{
    (void)context;
    (void)data;
    return byteCount;
}

int LLVMFuzzerInitialize(int* /*argc*/, char*** argv)
{
    std::string exe_path = CPLGetPath((*argv)[0]);
    int ret = chdir(exe_path.c_str());
    assert(ret == 0);
    if( CPLGetConfigOption("GDAL_DATA", nullptr) == nullptr )
    {
        CPLSetConfigOption("GDAL_DATA", exe_path.c_str());
    }
    CPLSetConfigOption("CPL_TMPDIR", "/tmp");
    msSetup();
    if( getenv("PROJ_LIB") == nullptr )
    {
        msSetPROJ_LIB(exe_path.c_str(), nullptr);
    }

    stdin_ctx.label = "null";
    stdin_ctx.write_channel = MS_FALSE;
    stdin_ctx.readWriteFunc = dummyRead;
    stdin_ctx.cbData = nullptr;

    stdout_ctx.label = "null";
    stdout_ctx.write_channel = MS_TRUE;
    stdout_ctx.readWriteFunc = dummyWrite;
    stdout_ctx.cbData = nullptr;

    stderr_ctx.label = "null";
    stderr_ctx.write_channel = MS_TRUE;
    stderr_ctx.readWriteFunc = dummyWrite;
    stderr_ctx.cbData = nullptr;

    msIO_installHandlers( &stdin_ctx, &stdout_ctx, &stderr_ctx );

    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len)
{
    mapObj* map = msLoadMap("query_string_fuzzer.map", nullptr);
    assert(map);

    mapservObj* mapserv = msAllocMapServObj();
    mapserv->map = map;
    mapserv->request->type = MS_GET_REQUEST;

    // Read query parameters from fuzzed buffer¨
    bool readKey = true;
    std::string key;
    std::string value;
    for( size_t i = 0; i < len; i++ )
    {
        char ch = (reinterpret_cast<const char*>(buf))[i];
        if( readKey )
        {
            if( ch == '\n' )
                readKey = false;
            else
                key += ch;
        }
        else
        {
            if( ch == '\n' )
            {
                mapserv->request->ParamNames = (char**)msSmallRealloc(
                    mapserv->request->ParamNames,sizeof(char *) * (mapserv->request->NumParams + 1));
                mapserv->request->ParamValues = (char**)msSmallRealloc(
                    mapserv->request->ParamValues,sizeof(char *) * (mapserv->request->NumParams + 1));
                mapserv->request->ParamNames[mapserv->request->NumParams] = msStrdup(key.c_str());
                mapserv->request->ParamValues[mapserv->request->NumParams] = msStrdup(value.c_str());
                mapserv->request->NumParams++;

                readKey = true;
                key.clear();
                value.clear();
            }
            else
                value += ch;
        }
    }

    msCGIDispatchRequest(mapserv);
    msFreeMapServObj(mapserv);
    return 0;
}
