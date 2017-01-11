//
// Created by lin-k on 01.10.2016.
//

#ifndef NESEMULATOR_EMULATOR_H
#define NESEMULATOR_EMULATOR_H

#include "Common.h"
#include "Console.h"

class Emulator {
public:
    Emulator(std::string _filename, void (*_drawPixel)(int x, int y, int color, void *userData), void (*_vsync)(void *userData), void *userData);

    void emitInput(bool states[8]);

    void preRun();

    void makeStep();

    void drawerFunc(int x, int y, int color);

    void vertSyncHandler();

    virtual ~Emulator();

protected:
private:
    void *userD;
    void (*drawPixel)(int x, int y, int color, void *userData);
    void (*vsync)(void *userData);
    bool isRunning;
    std::string filename;
};


#endif //NESEMULATOR_EMULATOR_H
