#include "common.h"

#include <stdio.h>
#include <stdlib.h>

#include "Format.h"
#include "Parameter.h"
#include "InputSys.h"
#include "VideoSys.h"

/* the "Info: status" and "Analog: Left" strings of a shared header */
INCLUDE_RODATA("asm/main_g/nonmatchings/Parameter", D_80019A1C);

/* the number as text (in Format.cpp) */
string ToString(float value);
string ToString(int value);

ParameterListener::~ParameterListener()
{
}

void Parameter::Notify()
{
    for (slist<ParameterListener*>::const_iterator i = m_listeners.begin(); i != m_listeners.end(); ++i) {
        (*i)->Changed(this);
    }
}

Parameter::Parameter(string name) : m_name(name)
{
}

Parameter::~Parameter()
{
}

void Parameter::AddListener(ParameterListener* listener)
{
    m_listeners.push_front(listener);
}

void Parameter::RemoveListener(ParameterListener* listener)
{
    m_listeners.remove(listener);
}

void Parameter::Increase()
{
    Notify();
}

void Parameter::Decrease()
{
    Notify();
}

string Parameter::ToString()
{
    return m_name;
}

ActionParameter::ActionParameter(string name) : Parameter(name), m_value(1)
{
}

void ActionParameter::Increase()
{
    m_value = 1;
    Notify();
}

void ActionParameter::Decrease()
{
    m_value = 0;
    Notify();
}

BoolParameter::BoolParameter(string name, int value) : Parameter(name), m_value(value)
{
}

void BoolParameter::Increase()
{
    m_value = 1;
    Notify();
}

void BoolParameter::Decrease()
{
    m_value = 0;
    Notify();
}

string BoolParameter::ToString()
{
    if (m_value) {
        return m_name + "=" + "true";
    }
    return m_name + "=" + "false";
}

ChoiceParameter::ChoiceParameter(string name, string choice) : Parameter(name), m_index(0), m_count(1)
{
    m_choices.push_back(choice);
}

void ChoiceParameter::AddChoice(string choice)
{
    m_count++;
    m_choices.push_back(choice);
}

void ChoiceParameter::Increase()
{
    if (m_index < m_count - 1) {
        m_index++;
        Notify();
    }
}

void ChoiceParameter::Decrease()
{
    if (m_index > 0) {
        m_index--;
        Notify();
    }
}

int ChoiceParameter::GetIndex()
{
    return m_index;
}

string ChoiceParameter::ToString()
{
    return m_name + "=" + m_choices[m_index];
}

void ParameterMenu::Rebuild()
{
    m_text = string("");
    for (list<Parameter*>::const_iterator i = m_parameters.begin(); i != m_parameters.end(); ++i) {
        if (m_current == i) {
            m_text += '>';
        } else {
            m_text += ' ';
        }
        m_text += (*i)->ToString();
        m_text += '\n';
    }
}

ParameterMenu::ParameterMenu(const char* padName) : m_padName(padName)
{
    m_current = m_parameters.end();
}

ParameterMenu::~ParameterMenu()
{
    for (list<Parameter*>::const_iterator i = m_parameters.begin(); i != m_parameters.end(); ++i) {
        (*i)->RemoveListener(this);
    }
}

void ParameterMenu::Add(Parameter* parameter)
{
    m_parameters.push_back(parameter);
    parameter->AddListener(this);
    Rebuild();
}

void ParameterMenu::Update()
{
    Buttons pressed = InputSys::GetPad(m_padName)->m_fourWay.pressed;
    if (pressed.up) {
        if (m_current != m_parameters.begin()) {
            --m_current;
        }
        Rebuild();
    } else if (pressed.down) {
        if (m_current != m_parameters.end()) {
            ++m_current;
        }
        Rebuild();
    } else if (pressed.left) {
        if (m_current != m_parameters.end()) {
            (*m_current)->Decrease();
        }
    } else if (pressed.right) {
        if (m_current != m_parameters.end()) {
            (*m_current)->Increase();
        }
    }
}

void ParameterMenu::Draw()
{
    if (InputSys::IsPlayerName(m_padName)) {
        VideoSys::Print(m_text.c_str());
    }
}

void ParameterMenu::Changed(Parameter* parameter)
{
    Rebuild();
}

template <class T>
void NumberParameter<T>::Clamp()
{
    if (m_hasMin && m_value < m_min) {
        m_value = m_min;
    }
    if (m_hasMax && m_value > m_max) {
        m_value = m_max;
    }
}

template <class T>
NumberParameter<T>::NumberParameter(string name, T value, T step)
    : Parameter(name), m_value(value), m_step(step), m_hasMin(0), m_hasMax(0)
{
}

template <class T>
NumberParameter<T>::NumberParameter(string name, T value, T step, T min)
    : Parameter(name), m_value(value), m_step(step), m_hasMin(1), m_min(min), m_hasMax(0)
{
}

template <class T>
NumberParameter<T>::NumberParameter(string name, T value, T step, T min, T max)
    : Parameter(name), m_value(value), m_step(step), m_hasMin(1), m_min(min), m_hasMax(1), m_max(max)
{
}

template <class T>
void NumberParameter<T>::Increase()
{
    m_value += m_step;
    Clamp();
    Notify();
}

template <class T>
void NumberParameter<T>::Decrease()
{
    m_value -= m_step;
    Clamp();
    Notify();
}

template <class T>
string NumberParameter<T>::ToString()
{
    return m_name + "=" + ::ToString(m_value);
}

/* the ones the game's menus use */
template void NumberParameter<float>::Clamp();
template NumberParameter<float>::NumberParameter(string name, float value, float step, float min);
template NumberParameter<float>::NumberParameter(string name, float value, float step);
template void NumberParameter<float>::Increase();
template void NumberParameter<float>::Decrease();
template string NumberParameter<float>::ToString();
template void NumberParameter<int>::Clamp();
template NumberParameter<int>::NumberParameter(string name, int value, int step, int min, int max);
template NumberParameter<int>::NumberParameter(string name, int value, int step);
template void NumberParameter<int>::Increase();
template void NumberParameter<int>::Decrease();
template string NumberParameter<int>::ToString();
