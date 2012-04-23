#ifndef ACTORLIB_HPP
#define ACTORLIB_HPP


#include <pthread.h>
#include <semaphore.h>
#include <list>


namespace actorlib {


/** result of an actor's computation.
    Value-type class.
    Not thread-safe; different values are thread-safe.
    @param R type of result.
 */
template <class R> class result {
public:
    /** the default constructor.
        @param v the default value of the result.
     */
    result(const R &v = R()) : m_data(new data(v)) {}

    /** the copy constructor.
        @param r source object.
     */
    result(const result<R> &r) : m_data(r.m_data) {
        m_data->inc_ref();
    }

    /** the destructor.
     */
    ~result() {
        m_data->dec_ref();
    }

    /** the assignment operator.
        @param r source object.
        @return reference to this.
     */
    result<R> &operator = (const result<R> &r) {
        r.m_data->inc_ref();
        m_data->dec_ref();
        m_data = r.m_data;
        return *this;
    }

    /** retrieves the value of the computation.
        It blocks until the result is available.
        @return the value of the computation.
     */
    R get() const {
        return m_data->get();
    }

    /** automatic conversion to value.
        It calls the get() function.
        @return the value of the computation.
     */
    operator R () const {
        return m_data->get();
    }

    /** sets the value.
        Any thread waiting on the result value will be awoken.
        @param v new value.
     */
    void set(const R &v) {
        m_data->set(v);
    }

    /** assignment from value.
        It calls the set(v) function.
        @param v new value.
        @return reference to this.
     */
    result<R> &operator = (const R &v) {
        m_data->set(v);
        return *this;
    }

private:
    //the internal result structure, shared by all threads
    struct data {
        //mutex
        pthread_mutex_t m_mutex;

        //wait condition
        pthread_cond_t m_cond;

        //reference count
        size_t m_ref_count;

        //result value
        R m_value;

        //if value is set
        bool m_value_set;

        //constructor
        data(const R &v) : m_value(v) {
            pthread_mutex_init(&m_mutex, NULL);
            pthread_cond_init(&m_cond, NULL);
            m_ref_count = 1;
            m_value_set = false;
        }

        //destructor
        ~data() {
            pthread_cond_destroy(&m_cond);
            pthread_mutex_destroy(&m_mutex);
        }

        //increment the reference count
        void inc_ref() {
            pthread_mutex_lock(&m_mutex);
            ++m_ref_count;
            pthread_mutex_unlock(&m_mutex);
        }

        //decrements the reference count and deletes the object if it reaches 0
        void dec_ref() {
            pthread_mutex_lock(&m_mutex);
            bool delete_this = --m_ref_count == 0;
            pthread_mutex_unlock(&m_mutex);
            if (delete_this) delete this;
        }

        //get the value
        R get() {
            pthread_mutex_lock(&m_mutex);
            if (!m_value_set) pthread_cond_wait(&m_cond, &m_mutex);
            R r = m_value;
            pthread_mutex_unlock(&m_mutex);
            return r;
        }

        //set the value
        void set(const R &v) {
            pthread_mutex_lock(&m_mutex);
            m_value = v;
            m_value_set = true;
            pthread_mutex_unlock(&m_mutex);
            pthread_cond_signal(&m_cond);
        }
    };

    //internal pointer to data
    data *m_data;
};


/** specialization for void result.
 */
template <> class result<void> {
public:
};


/** the base class for all actors.

    It provides the interface for posting messages to actors.

    The messages are entered in queue, and then the actor's thread
    is woken up to process the messages.

    The messages are calls to the internal functions of the object.
 */
class actor {
public:
    /** constructs an actor.
        The internal thread is started.
     */
    actor();

    /** destroys an actor.
        The calling thread blocks until the actor thread is terminated.
     */
    ~actor();

protected:
    /** puts a message with 0 parameters.
        @param f function to put.
     */
    template <class C, class R> result<R> put(R (C::*f)()) {
        result<R> r;
        put(new object_message_0<C, R>(static_cast<C *>(this), r, f));
        return r;
    }

    /** puts a message with 1 parameter.
        @param f function to put.
        @param t1 1st argument.
     */
    template <class C, class R, class T1> result<R> put(R (C::*f)(const T1 &), const T1 &t1) {
        result<R> r;
        put(new object_message_1<C, R, T1>(static_cast<C *>(this), r, f, t1));
        return r;
    }

    /** puts a message with 2 parameters.
        @param f function to put.
        @param t1 1st argument.
        @param t2 2nd argument.
     */
    template <class C, class R, class T1, class T2> result<R> put(R (C::*f)(const T1 &, const T2 &), const T1 &t1, const T2 &t2) {
        result<R> r;
        put(new object_message_2<C, R, T1, T2>(static_cast<C *>(this), r, f, t1, t2));
        return r;
    }

    /** puts the exit message in the message loop.
        If this message is executed, the loop is terminated.
     */
    void exit() {
        put(&actor::_exit);
    }

private:
    //invoke object with a non-void result
    template <class C, class R> class invoker {
    public:
        //invoke with 0 params
        static void exec(result<R> &r, C *o, R (C::*f)()) {
            r = (o->*f)();
        }

        //invoke with 1 param
        template <class T1> static void exec(result<R> &r, C *o, R (C::*f)(const T1 &), const T1 &t1) {
            r = (o->*f)(t1);
        }

        //invoke with 2 params
        template <class T1, class T2>
        static void exec(result<void> &r, C *o,
            void (C::*f)(const T1 &, const T2 &),
            const T1 &t1, const T2 &t2)
        {
            r = (o->*f)(t1, t2);
        }

        //invoke with 3 params
        template <class T1, class T2, class T3>
        static void exec(result<void> &r, C *o,
            void (C::*f)(const T1 &, const T2 &, const T3 &),
            const T1 &t1, const T2 &t2, const T3 &t3)
        {
            r = (o->*f)(t1, t2, t3);
        }

        //invoke with 4 params
        template <class T1, class T2, class T3, class T4>
        static void exec(result<void> &r, C *o,
            void (C::*f)(const T1 &, const T2 &, const T3 &, const T4 &),
            const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4)
        {
            r = (o->*f)(t1, t2, t3, t4);
        }

        //invoke with 5 params
        template <class T1, class T2, class T3, class T4, class T5>
        static void exec(result<void> &r, C *o,
            void (C::*f)(const T1 &, const T2 &, const T3 &, const T4 &, const T5 &),
            const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4, const T5 &t5)
        {
            r = (o->*f)(t1, t2, t3, t4, t5);
        }

        //invoke with 6 params
        template <class T1, class T2, class T3, class T4, class T5, class T6>
        static void exec(result<void> &r, C *o,
            void (C::*f)(const T1 &, const T2 &, const T3 &, const T4 &, const T5 &, const T6 &),
            const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4, const T5 &t5, const T6 &t6)
        {
            r = (o->*f)(t1, t2, t3, t4, t5, t6);
        }

        //invoke with 7 params
        template <class T1, class T2, class T3, class T4, class T5, class T6, class T7>
        static void exec(result<void> &r, C *o,
            void (C::*f)(const T1 &, const T2 &, const T3 &, const T4 &, const T5 &, const T6 &, const T7 &),
            const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4, const T5 &t5, const T6 &t6, const T7 &t7)
        {
            r = (o->*f)(t1, t2, t3, t4, t5, t6, t7);
        }

        //invoke with 8 params
        template <class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8>
        static void exec(result<void> &r, C *o,
            void (C::*f)(const T1 &, const T2 &, const T3 &, const T4 &, const T5 &, const T6 &, const T7 &, const T8 &),
            const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4, const T5 &t5, const T6 &t6, const T7 &t7, const T8 &t8)
        {
            r = (o->*f)(t1, t2, t3, t4, t5, t6, t7, t8);
        }

        //invoke with 9 params
        template <class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9>
        static void exec(result<void> &r, C *o,
            void (C::*f)(const T1 &, const T2 &, const T3 &, const T4 &, const T5 &, const T6 &, const T7 &, const T8 &, const T9 &),
            const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4, const T5 &t5, const T6 &t6, const T7 &t7, const T8 &t8, const T9 &t9)
        {
            r = (o->*f)(t1, t2, t3, t4, t5, t6, t7, t8, t9);
        }
    };

    //specialization for void
    template <class C> class invoker<C, void> {
    public:
        //invoke with 0 params
        static void exec(result<void> &r, C *o, void (C::*f)()) {
            (o->*f)();
        }

        //invoke with 1 param
        template <class T1> static void exec(result<void> &r, C *o, void (C::*f)(const T1 &), const T1 &t1) {
            (o->*f)(t1);
        }

        //invoke with 2 params
        template <class T1, class T2>
        static void exec(result<void> &r, C *o,
            void (C::*f)(const T1 &, const T2 &),
            const T1 &t1, const T2 &t2)
        {
            (o->*f)(t1, t2);
        }

        //invoke with 3 params
        template <class T1, class T2, class T3>
        static void exec(result<void> &r, C *o,
            void (C::*f)(const T1 &, const T2 &, const T3 &),
            const T1 &t1, const T2 &t2, const T3 &t3)
        {
            (o->*f)(t1, t2, t3);
        }

        //invoke with 4 params
        template <class T1, class T2, class T3, class T4>
        static void exec(result<void> &r, C *o,
            void (C::*f)(const T1 &, const T2 &, const T3 &, const T4 &),
            const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4)
        {
            (o->*f)(t1, t2, t3, t4);
        }

        //invoke with 5 params
        template <class T1, class T2, class T3, class T4, class T5>
        static void exec(result<void> &r, C *o,
            void (C::*f)(const T1 &, const T2 &, const T3 &, const T4 &, const T5 &),
            const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4, const T5 &t5)
        {
            (o->*f)(t1, t2, t3, t4, t5);
        }

        //invoke with 6 params
        template <class T1, class T2, class T3, class T4, class T5, class T6>
        static void exec(result<void> &r, C *o,
            void (C::*f)(const T1 &, const T2 &, const T3 &, const T4 &, const T5 &, const T6 &),
            const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4, const T5 &t5, const T6 &t6)
        {
            (o->*f)(t1, t2, t3, t4, t5, t6);
        }

        //invoke with 7 params
        template <class T1, class T2, class T3, class T4, class T5, class T6, class T7>
        static void exec(result<void> &r, C *o,
            void (C::*f)(const T1 &, const T2 &, const T3 &, const T4 &, const T5 &, const T6 &, const T7 &),
            const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4, const T5 &t5, const T6 &t6, const T7 &t7)
        {
            (o->*f)(t1, t2, t3, t4, t5, t6, t7);
        }

        //invoke with 8 params
        template <class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8>
        static void exec(result<void> &r, C *o,
            void (C::*f)(const T1 &, const T2 &, const T3 &, const T4 &, const T5 &, const T6 &, const T7 &, const T8 &),
            const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4, const T5 &t5, const T6 &t6, const T7 &t7, const T8 &t8)
        {
            (o->*f)(t1, t2, t3, t4, t5, t6, t7, t8);
        }

        //invoke with 9 params
        template <class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9>
        static void exec(result<void> &r, C *o,
            void (C::*f)(const T1 &, const T2 &, const T3 &, const T4 &, const T5 &, const T6 &, const T7 &, const T8 &, const T9 &),
            const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4, const T5 &t5, const T6 &t6, const T7 &t7, const T8 &t8, const T9 &t9)
        {
            (o->*f)(t1, t2, t3, t4, t5, t6, t7, t8, t9);
        }
    };

    //a message
    class message {
    public:
        //virtual destructor due to virtual implementation.
        virtual ~message() {}

        //interface for executing the message
        virtual void exec() = 0;
    };

    //a message with a specific target object and result
    template <class C, class R> class object_message : public message {
    public:
        //object
        C *m_object;

        //result variable
        result<R> m_result;

        //constructor.
        object_message(C *object, const result<R> &r) :
            m_object(object), m_result(r) {}
    };

    //a message with zero parameters
    template <class C, class R> class object_message_0 : public object_message<C, R> {
    public:
        //function type
        typedef R (C::*function)();

        //function
        function m_function;

        //constructor.
        object_message_0(C *object, const result<R> &r, function f) :
            object_message<C, R>(object, r), m_function(f) {}

        //calls the function
        virtual void exec() {
            invoker<C, R>::exec(this->m_result, this->m_object, this->m_function);
        }
    };

    //a message with 1 parameter
    template <class C, class R, class T1>
    class object_message_1 :
        public object_message<C, R>
    {
    public:
        //function type
        typedef R (C::*function)(const T1 &);

        //function
        function m_function;

        //argument 1
        T1 m_t1;

        //constructor.
        object_message_1(C *object, const result<R> &r, function f, const T1 &t1) :
            object_message<C, R>(object, r), m_function(f), m_t1(t1) {}

        //calls the function
        virtual void exec() {
            invoker<C, R>::exec(this->m_result, this->m_object, this->m_function, m_t1);
        }
    };

    //a message with 2 parameters
    template <class C, class R, class T1, class T2>
    class object_message_2 :
        public object_message<C, R>
    {
    public:
        //function type
        typedef R (C::*function)(const T1 &, const T2 &);

        //function
        function m_function;

        //arguments
        T1 m_t1;
        T2 m_t2;

        //constructor.
        object_message_2(C *object, const result<R> &r, function f, const T1 &t1, const T2 &t2) :
            object_message<C, R>(object, r), m_function(f), m_t1(t1), m_t2(t2) {}

        //calls the function
        virtual void exec() {
            invoker<C, R>::exec(this->m_result, this->m_object, this->m_function, m_t1, m_t2);
        }
    };

    //a message with 3 parameters
    template <class C, class R, class T1, class T2, class T3>
    class object_message_3 :
        public object_message<C, R>
    {
    public:
        //function type
        typedef R (C::*function)(const T1 &, const T2 &, const T3 &);

        //function
        function m_function;

        //arguments
        T1 m_t1;
        T2 m_t2;
        T3 m_t3;

        //constructor.
        object_message_3(C *object, const result<R> &r, function f, const T1 &t1, const T2 &t2, const T3 &t3) :
            object_message<C, R>(object, r), m_function(f), m_t1(t1), m_t2(t2), m_t3(t3) {}

        //calls the function
        virtual void exec() {
            invoker<C, R>::exec(this->m_result, this->m_object, this->m_function, m_t1, m_t2, m_t3);
        }
    };

    //a message with 4 parameters
    template <class C, class R, class T1, class T2, class T3, class T4>
    class object_message_4 :
        public object_message<C, R>
    {
    public:
        //function type
        typedef R (C::*function)(const T1 &, const T2 &, const T3 &, const T4 &);

        //function
        function m_function;

        //arguments
        T1 m_t1;
        T2 m_t2;
        T3 m_t3;
        T4 m_t4;

        //constructor.
        object_message_4(C *object, const result<R> &r, function f, const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4) :
            object_message<C, R>(object, r), m_function(f), m_t1(t1), m_t2(t2), m_t3(t3), m_t4(t4) {}

        //calls the function
        virtual void exec() {
            invoker<C, R>::exec(this->m_result, this->m_object, this->m_function, m_t1, m_t2, m_t3, m_t4);
        }
    };

    //a message with 5 parameters
    template <class C, class R, class T1, class T2, class T3, class T4, class T5>
    class object_message_5 :
        public object_message<C, R>
    {
    public:
        //function type
        typedef R (C::*function)(const T1 &, const T2 &, const T3 &, const T4 &, const T5 &);

        //function
        function m_function;

        //arguments
        T1 m_t1;
        T2 m_t2;
        T3 m_t3;
        T4 m_t4;
        T5 m_t5;

        //constructor.
        object_message_5(C *object, const result<R> &r, function f, const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4, const T5 &t5) :
            object_message<C, R>(object, r), m_function(f), m_t1(t1), m_t2(t2), m_t3(t3), m_t4(t4), m_t5(t5) {}

        //calls the function
        virtual void exec() {
            invoker<C, R>::exec(this->m_result, this->m_object, this->m_function, m_t1, m_t2, m_t3, m_t4, m_t5);
        }
    };

    //a message with 6 parameters
    template <class C, class R, class T1, class T2, class T3, class T4, class T5, class T6>
    class object_message_6 :
        public object_message<C, R>
    {
    public:
        //function type
        typedef R (C::*function)(const T1 &, const T2 &, const T3 &, const T4 &, const T5 &, const T6 &);

        //function
        function m_function;

        //arguments
        T1 m_t1;
        T2 m_t2;
        T3 m_t3;
        T4 m_t4;
        T5 m_t5;
        T6 m_t6;

        //constructor.
        object_message_6(C *object, const result<R> &r, function f, const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4, const T5 &t5, const T6 &t6) :
            object_message<C, R>(object, r), m_function(f), m_t1(t1), m_t2(t2), m_t3(t3), m_t4(t4), m_t5(t5), m_t6(t6) {}

        //calls the function
        virtual void exec() {
            invoker<C, R>::exec(this->m_result, this->m_object, this->m_function, m_t1, m_t2, m_t3, m_t4, m_t5, m_t6);
        }
    };

    //a message with 7 parameters
    template <class C, class R, class T1, class T2, class T3, class T4, class T5, class T6, class T7>
    class object_message_7 :
        public object_message<C, R>
    {
    public:
        //function type
        typedef R (C::*function)(const T1 &, const T2 &, const T3 &, const T4 &, const T5 &, const T6 &, const T7 &);

        //function
        function m_function;

        //arguments
        T1 m_t1;
        T2 m_t2;
        T3 m_t3;
        T4 m_t4;
        T5 m_t5;
        T6 m_t6;
        T7 m_t7;

        //constructor.
        object_message_7(C *object, const result<R> &r, function f, const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4, const T5 &t5, const T6 &t6, const T7 &t7) :
            object_message<C, R>(object, r), m_function(f), m_t1(t1), m_t2(t2), m_t3(t3), m_t4(t4), m_t5(t5), m_t6(t6), m_t7(t7) {}

        //calls the function
        virtual void exec() {
            invoker<C, R>::exec(this->m_result, this->m_object, this->m_function, m_t1, m_t2, m_t3, m_t4, m_t5, m_t6, m_t7);
        }
    };

    //a message with 8 parameters
    template <class C, class R, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8>
    class object_message_8 :
        public object_message<C, R>
    {
    public:
        //function type
        typedef R (C::*function)(const T1 &, const T2 &, const T3 &, const T4 &, const T5 &, const T6 &, const T7 &, const T8 &);

        //function
        function m_function;

        //arguments
        T1 m_t1;
        T2 m_t2;
        T3 m_t3;
        T4 m_t4;
        T5 m_t5;
        T6 m_t6;
        T7 m_t7;
        T8 m_t8;

        //constructor.
        object_message_8(C *object, const result<R> &r, function f, const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4, const T5 &t5, const T6 &t6, const T7 &t7, const T8 &t8) :
            object_message<C, R>(object, r), m_function(f), m_t1(t1), m_t2(t2), m_t3(t3), m_t4(t4), m_t5(t5), m_t6(t6), m_t7(t7), m_t8(t8) {}

        //calls the function
        virtual void exec() {
            invoker<C, R>::exec(this->m_result, this->m_object, this->m_function, m_t1, m_t2, m_t3, m_t4, m_t5, m_t6, m_t7, m_t8);
        }
    };

    //a message with 9 parameters
    template <class C, class R, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9>
    class object_message_9 :
        public object_message<C, R>
    {
    public:
        //function type
        typedef R (C::*function)(const T1 &, const T2 &, const T3 &, const T4 &, const T5 &, const T6 &, const T7 &, const T8 &, const T9 &);

        //function
        function m_function;

        //arguments
        T1 m_t1;
        T2 m_t2;
        T3 m_t3;
        T4 m_t4;
        T5 m_t5;
        T6 m_t6;
        T7 m_t7;
        T8 m_t8;
        T9 m_t9;

        //constructor.
        object_message_9(C *object, const result<R> &r, function f, const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4, const T5 &t5, const T6 &t6, const T7 &t7, const T8 &t8, const T9 &t9) :
            object_message<C, R>(object, r), m_function(f), m_t1(t1), m_t2(t2), m_t3(t3), m_t4(t4), m_t5(t5), m_t6(t6), m_t7(t7), m_t8(t8), m_t9(t9) {}

        //calls the function
        virtual void exec() {
            invoker<C, R>::exec(this->m_result, this->m_object, this->m_function, m_t1, m_t2, m_t3, m_t4, m_t5, m_t6, m_t7, m_t8, m_t9);
        }
    };

    //type message ptr
    typedef message *message_ptr;

    //type of message list
    typedef std::list<message_ptr> message_list;

    //loop flag
    bool m_loop;

    //mutex used for synchronization over the message list
    pthread_mutex_t m_mutex;

    //semaphore used for counting the messages of the message list
    sem_t m_sem;

    //thread handle
    pthread_t m_thread;

    //messages
    message_list m_messages;

    //not copyable
    actor(const actor &);
    actor &operator = (const actor &);

    //puts a message in the message queue, synchronized
    void put(message *msg);

    //exit
    void _exit();

    //the message handling loop
    void run();

    //internal function which calls the thread's run function
    static void *thread_proc(void *arg);
};


} //namespace actorlib


#endif //ACTORLIB_HPP
