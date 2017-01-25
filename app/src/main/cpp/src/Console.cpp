//
// Created by lin-k on 15.10.2016.
//

#include "../include/Console.h"

void Console::init(std::string filename, void (*_drawPixel)(int x, int y, int color, void *userData), void (*_vsync)(void *userData), void *userData) {
    drawPixel = _drawPixel;
    vsync = _vsync;
    userD = userData;
    rom = new ROM();
    ppu = new PPU();
    cpu = new CPU();
    mem = new Memory();
    controller = new Controller();
    rom->init(filename);
    mem->init();
    cpu->init();
    ppu->init();
}

void Console::callVSync() {
    vsync(userD);
}

void Console::callPutPixel(int x, int y, int color) {
    drawPixel(x, y, color, userD);
}

void Console::step() {
    int cycles = cpu->execute();
    if (mem->addCyclesAfterDMA == 513) { // kostyl, for better synchronization
        cycles += mem->addCyclesAfterDMA;
        mem->addCyclesAfterDMA = 0;
    }

    for (int i = 0; i < cycles * 3; i++) {
        ppu->execute();
        rom->execute();
    }
}

Controller *Console::getController() {
    return controller;
}

CPU *Console::getCPU() {
    return cpu;
}

PPU *Console::getPPU() {
    return ppu;
}

Memory *Console::getMemory() {
    return mem;
}

ROM *Console::getROM() {
    return rom;
}