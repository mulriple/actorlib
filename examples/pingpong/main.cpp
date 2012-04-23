#include <cstdio>
#include <iostream>
#include <string>
#include <sstream>
#include "actorlib.hpp"
using namespace std;
using namespace actorlib;


//the console actor
class console : public actor {
public:
    //prints the following message
    void print(const string &s) {
        put(&console::_print, s);
    }

private:
    //internal print
    void _print(const string &s) {
        cout << s;
    }
};


//an integer actor
class integer : public actor {
public:
    //constructor
    integer(int v = 0) : m_value(v) {
    }

    //get the value
    result<int> get() {
        return put(&integer::_get);
    }

    //set the value
    void set(int v) {
        put(&integer::_set, v);
    }

private:
    //value
    int m_value;

    //internal get
    int _get() {
        return m_value;
    }

    //internal set
    void _set(const int &v) {
        m_value = v;
    }
};


class pong;


//the ping actor.
class ping : public actor {
public:
    //constructor
    ping(console &c, integer &v, pong &p) :
        m_console(&c), m_value(&v), m_pong(&p) {}

    //does ping, and then calls pong, until the value is 0
    void do_ping() {
        put(&ping::_do_ping);
    }

private:
    //members
    console *m_console;
    integer *m_value;
    pong *m_pong;

    //internal ping
    void _do_ping();
};


//the pong actor.
class pong : public actor {
public:
    //constructor
    pong(console &c, integer &v, ping &p) :
        m_console(&c), m_value(&v), m_ping(&p) {}

    //does pong, and then calls ping, until the value is 0
    void do_pong() {
        put(&pong::_do_pong);
    }

private:
    //members
    console *m_console;
    integer *m_value;
    ping *m_ping;

    //internal pong
    void _do_pong();
};


//internal ping
void ping::_do_ping() {
    int v = m_value->get();
    if (v > 0) {
        stringstream stream;
        stream << v << ": pong\n";
        m_console->print(stream.str());
        m_value->set(v - 1);
        m_pong->do_pong();
    }
}


//internal pong
void pong::_do_pong() {
    int v = m_value->get();
    if (v > 0) {
        stringstream stream;
        stream << v << ": ping\n";
        m_console->print(stream.str());
        m_value->set(v - 1);
        m_ping->do_ping();
    }
}


//the objects
console console_;
integer value(100);
extern ping ping_;
pong pong_(console_, value, ping_);
ping ping_(console_, value, pong_);


int main() {
    cout << "press any key to exit...\n";
    pong_.do_pong();
    getchar();
    return 0;
}
