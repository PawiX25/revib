#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "Message.h"
#include "FileSys.h"
#include "VideoSys.h"

Message* Message::s_instance;

void Message::Show()
{
    VideoSys::Flip(1);
    Message* message = s_instance;
    message->m_camera.Update(0);
    message->m_movie.Draw();
}

Message::Message() : AnimatedModel("loading/loading.tmd", "loading/loading.anm", "loading/loading.anc", 1, 0.0)
{
#line 28 "C:/psj/dev/locale/game/Message.cpp"
    assert(s_instance == NULL);
#line 26 "src/game/Message.cpp"
    s_instance = this;
    FileSys::SetReadCallback(Show);
    Show();
    m_model->SetJitter(15.0, 1);
    Show();
    Show();
}

Message::~Message()
{
    FileSys::SetReadCallback(NULL);
    s_instance = NULL;
}
