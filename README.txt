Introduction
------------

The two files actorlib.hpp and actorlib.cpp contain an object-oriented version
of the Actor model for C++.

The Actor model is a solution for concurrency: each thread is an actor that
receives messages. A message reception loop runs a pattern matching function
on the received data and selects the appropriate block of code to run, based
on the type or content of data.

This library deviates a little from the above: each object is a thread,
but instead of receiving messages, it receives pointers to its own methods,
to be executed in the context of the actor object.

The library uses pthreads for threading.

Example
-------

The following example illustrates the use of actors. It has two main actors,
the actor Pong, which prints 'pong' and then calls the actor Ping, which
prints 'ping' and calls the actor Pong.

There are two other secondary actors:

1) an integer actor, i.e. an actor which holds an integer value. 
This actor is used as a loop counter by the Ping and Pong classes. 
Once this value reaches 0, then the Ping and Pong classes stop calling 
each other.

2) a console actor, which is used to serialize access to cout.

The Console Actor
-----------------

Let's examine the console actor first, which is the simplest:

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

We can see the following:

-it inherits from class actor.
-it has a public method 'print' which puts an internal method '_print' in the queue.

The Integer Actor
-----------------

Let's see the integer actor, which is more complicated:

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

The most interesting parts of the above class are:

-it has a 'get()' function which returns a result<int>, instead of int.
-it has a 'set(n)' function, which calls an internal '_set' function.

The class result<T> is used to get a future result from an actor.
The caller will get this variable and then may proceed to do other things.
The caller will be blocked only when it requests the actual value in the result.
If the target actor will have computed the result, the caller will return the result,
otherwise it will be blocked.

The Ping Class
--------------

Let's see how the above two classes are used in the Ping class:

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
    
The most interesting aspect is the '_do_ping' method:

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

As we can see, from the above method:

1) the value actor's internal value is retrieved by 'get()'. The caller blocks on this value.
2) the console is invoked to print a message.
3) the value is then invoked to decrement its internal value.
4) finally, the Pong instance is invoked.

The Pong Class
--------------

The Pong class is exactly the same as the Ping class:

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

Output
------

The output of the example is the following:

    press any key to exit...
    100: ping
    99: pong
    98: ping
    97: pong
    96: ping
    95: pong
    94: ping
    93: pong
    92: ping
    91: pong
    90: ping
    89: pong
    88: ping
    87: pong
    86: ping
    85: pong
    84: ping
    83: pong
    82: ping
    81: pong
    80: ping
    79: pong
    78: ping
    77: pong
    76: ping
    75: pong
    74: ping
    73: pong
    72: ping
    71: pong
    70: ping
    69: pong
    68: ping
    67: pong
    66: ping
    65: pong
    64: ping
    63: pong
    62: ping
    61: pong
    60: ping
    59: pong
    58: ping
    57: pong
    56: ping
    55: pong
    54: ping
    53: pong
    52: ping
    51: pong
    50: ping
    49: pong
    48: ping
    47: pong
    46: ping
    45: pong
    44: ping
    43: pong
    42: ping
    41: pong
    40: ping
    39: pong
    38: ping
    37: pong
    36: ping
    35: pong
    34: ping
    33: pong
    32: ping
    31: pong
    30: ping
    29: pong
    28: ping
    27: pong
    26: ping
    25: pong
    24: ping
    23: pong
    22: ping
    21: pong
    20: ping
    19: pong
    18: ping
    17: pong
    16: ping
    15: pong
    14: ping
    13: pong
    12: ping
    11: pong
    10: ping
    9: pong
    8: ping
    7: pong
    6: ping
    5: pong
    4: ping
    3: pong
    2: ping
    1: pong
    
Conclusion
----------

In the above example, the execution seems to be sequencial, however, 
there are 4 threads involved, and there is no synchronization primitive used anywhere.
    
This is the power of the Actor model: it makes concurrency very easy, and if done in
an object-oriented manner, it makes concurrency even easier to use.
