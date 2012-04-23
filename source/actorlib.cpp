#include <cassert>
#include "actorlib.hpp"


namespace actorlib {


/** constructs an actor.
    The internal thread is started.
 */
actor::actor() {
    m_loop = true;
    pthread_mutex_init(&m_mutex, NULL);
    sem_init(&m_sem, 0, 0);
    pthread_create(&m_thread, NULL, thread_proc, this);
}


/** destroys an actor.
    The calling thread blocks until the actor thread is terminated.
 */
actor::~actor() {
    exit();
    pthread_join(m_thread, NULL);
    sem_destroy(&m_sem);
    pthread_mutex_destroy(&m_mutex);    
}


//puts a message in the message queue, synchronized
void actor::put(message *msg) {
    pthread_mutex_lock(&m_mutex);
    m_messages.push_back(msg);
    pthread_mutex_unlock(&m_mutex);    
    sem_post(&m_sem);
}


//exit
void actor::_exit() {
    m_loop = false;
}


//the message handling loop
void actor::run() {
    //while the loop is active
    while (m_loop) {
        //wait for message
        sem_wait(&m_sem);
        
        //get a message (synchronized block)
        pthread_mutex_lock(&m_mutex);
        assert(!m_messages.empty());
        message_ptr msg = m_messages.front();
        m_messages.pop_front();
        pthread_mutex_unlock(&m_mutex);
        
        //execute the message
        msg->exec();
        delete msg;
    }
}


//internal function which calls the thread's run function
void *actor::thread_proc(void *arg) {
    reinterpret_cast<actor *>(arg)->run();
    return 0;
}


} //namespace actorlib
