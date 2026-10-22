#include "common.h"

#include <stdlib.h>

#include "Format.h"
#include "Game.h"
#include "AudioStream.h"
#include "MemorySys.h"
#include <string> /* nothing here uses it, but its empty string is in the file's small data */
#include "SignalSys.h"
#include "Gameplay.h"

/* the allocator's "out of memory" string, of a shared header */
INCLUDE_RODATA("asm/main_g/nonmatchings/Game", D_80019B9C);

Game::Game()
{
    m_position = AudioStream::GetPosition();
    m_distance = 10.0f;
    m_vibriPosition = 1.25f;
    m_beatLength = AudioStream::SampleRate() * 0.5;
    m_beatRate = 1.0 / m_beatLength;
    SignalSys::Init(m_distance, m_vibriPosition, AudioStream::GetPosition());
    Gameplay::Init(m_distance, m_vibriPosition, m_beatRate * AudioStream::SampleRate());
    AudioStream::Start();
}

Game::~Game()
{
    AudioStream::Stop();
    Gameplay::Quit();
    SignalSys::Quit();
}

void Game::Update(int position)
{
    m_position = position;
    Gameplay::Update(position);
    SignalSys::Update(position);
}

Game* GameSys::s_game = 0;

void GameSys::Init()
{
    s_game = new Game;
}

void GameSys::Draw()
{
}

void GameSys::Update(int position)
{
    s_game->Update(position);
}

void GameSys::Quit()
{
    delete s_game;
    s_game = 0;
}
