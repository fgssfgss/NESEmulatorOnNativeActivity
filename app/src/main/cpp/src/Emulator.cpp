//
// Created by lin-k on 01.10.2016.
//

#include "../include/Emulator.h"
#include <jni.h>

Emulator::Emulator(std::string _filename, void (*_drawPixel)(int x, int y, int color, void *userData), void (*_vsync)(void *userData), void *userData) : userD(userData), filename(_filename), isRunning(true), drawPixel(_drawPixel), vsync(_vsync) {
}

void Emulator::drawerFunc(int x, int y, int color) {
    drawPixel(x, y, color, userD);
}

void Emulator::vertSyncHandler() {
    vsync(userD);
}

void Emulator::emitInput(bool states[8]) {
    //bool states[8] = {false, false, false, false, false, false, false, false};
    Console &c = Console::Instance();
    c.getController()->setButtons(states);
}

void Emulator::makeStep() {
    Console &c = Console::Instance();
    c.step();
}

void Emulator::preRun() {

    isRunning = true;
    auto f1 = std::bind(&Emulator::drawerFunc, this, std::placeholders::_1, std::placeholders::_2,
                        std::placeholders::_3);
    auto f2 = std::bind(&Emulator::vertSyncHandler, this);

    Console &c = Console::Instance();
    c.init(filename, f1, f2);
    return;
}

Emulator::~Emulator() {
}