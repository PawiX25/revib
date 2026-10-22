#ifndef PARAMETER_H
#define PARAMETER_H

#include "common.h"

#include "MemorySys.h"

#include <string>
#include <slist>
#include <vector>
#include <list>

class Parameter;

/* Told when a parameter changes. */
class ParameterListener {
public:
    virtual ~ParameterListener();
    virtual void Changed(Parameter* parameter) = 0;
};

/*
 * A named setting of the debug menus, stepped up and down with the
 * controller. Its listeners are told of every change.
 */
class Parameter {
public:
    Parameter(string name);
    virtual ~Parameter();

    void AddListener(ParameterListener* listener);
    void RemoveListener(ParameterListener* listener);

    virtual void Increase();
    virtual void Decrease();
    /* how the menu shows it */
    virtual string ToString();

protected:
    void Notify();

    string m_name;
    slist<ParameterListener*> m_listeners;
};

/* Something to do ("Send ... Effect"): set when stepped up, cleared when stepped down. */
class ActionParameter : public Parameter {
public:
    ActionParameter(string name);

    void Increase();
    void Decrease();

    int m_value;
};

class BoolParameter : public Parameter {
public:
    BoolParameter(string name, int value);

    void Increase();
    void Decrease();
    string ToString();

    int m_value;
};

/* One of a list of names. */
class ChoiceParameter : public Parameter {
public:
    ChoiceParameter(string name, string choice);

    void AddChoice(string choice);
    void Increase();
    void Decrease();
    int GetIndex();
    string ToString();

private:
    vector<string> m_choices;
    int m_index;
    int m_count;
};

/* A number stepped by `step`, kept within its limits if it has any. */
template <class T>
class NumberParameter : public Parameter {
public:
    NumberParameter(string name, T value, T step);
    NumberParameter(string name, T value, T step, T min);
    NumberParameter(string name, T value, T step, T min, T max);

    void Increase();
    void Decrease();
    string ToString();

    T m_value;

private:
    void Clamp();

    T m_step;
    int m_hasMin;
    T m_min;
    int m_hasMax;
    T m_max;
};

/*
 * A menu of parameters, printed on the screen while the named controller
 * is a player's: up and down choose one, left and right step it.
 */
class ParameterMenu : public ParameterListener {
public:
    ParameterMenu(const char* padName);
    ~ParameterMenu();

    void Add(Parameter* parameter);
    void Update();
    void Draw();
    void Changed(Parameter* parameter);

private:
    void Rebuild();

    const char* m_padName;
    string m_text;
    list<Parameter*> m_parameters;
    list<Parameter*>::const_iterator m_current;
};

#endif
