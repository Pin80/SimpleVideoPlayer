#include "video_reader.h"
#include <stdio.h>
#include <pthread.h>
#include "mainwindow.h"
#include "common.h"
#include "application.h"

// Программа написана на с++11, но в стиле: без исключений
// так как используются только Си библиотеки
// The Application was written on C++11 without exceptions
// because only C library is used.
//const char * cfn = "sample.mp4";

int main(int _argc, char *_argv[])
{
    TApplication a(_argc, _argv);
    return a.exec();
}
