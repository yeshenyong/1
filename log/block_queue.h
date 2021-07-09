#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H
#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include "../lock/locker.h"
using namespace std;

template <class T>
class block_queue
{
    public:
        block_queue( int max_size = 1000 )
        {
            if ( max_size <= 0 )
                exit(-1);

            //构造函数循环数组
            m_max_size = max_size;
            m_array = new T[max_size];
            m_size = 0;
            m_front = -1;
            m_back = -1;
        }
        void clear()
        {
            m_mutex.lock();
            m_size = 0;
            m_front = -1;
            m_back = -1;
            m_mutex.unlock();
        }
        ~block_queue()
        {
            m_mutex.lock();
            if ( m_array != NULL )
                delete [] m_array;
            m_mutex.unlock();
        }
        //判断队列是否为满
        bool full()
        {
            m_mutex.lock();
            if (m_size >= m_max_size)
            {
                m_mutex.unlock();
                return true;
            }
            m_mutex.unlock();
            return false;
        }
        //判断队列是否为空
        bool empty()
        {
            m_mutex.lock();
            if ( 0 == m_size )
            {
                m_mutex.unlock();
                return true;
            }
            m_mutex.unlock();
            return false;
        }
        //返回队首元素，之后try一下，使用queue是否比循环数组好
        //因为%运算量大，但是queue底层是vector
        bool front(T &value)
        {
            m_mutex.lock();
            if ( 0 == m_size )
            {
                m_mutex.unlock();
                return false;
            }
            value = m_array[m_front];
            m_mutex.unlock();
            return true;
        }
        bool back(T &value)
        {
            m_mutex.lock();
            if ( 0 == m_size )
            {
                m_mutex.unlock();
                return false;
            }
            value = m_array[m_back];
            m_mutex.unlock();
            return true;
        }
        int size()
        {
            int tmp = 0;
            m_mutex.lock();
            tmp = m_size;
            m_mutex.unlock();
            return tmp;
        }
        int max_size()
        {
            int tmp = 0;
            m_mutex.lock();
            tmp = m_max_size;
            m_mutex.unlock();
            return tmp; 
        }
        /*
            往队列中添加元素，需要将所有使用队列的线程先唤醒
            当有元素push进队列，相当于生产者生产了一个元素
            若当前没有线程等待条件变量，则唤醒无意义
        */
       bool push( const T &item )
       {
           m_mutex.lock();
           if ( m_size >= m_max_size )
           {
               m_cond.broadcast();
               m_mutex.unlock();
               return false;
           }
           m_back = ( m_back + 1 ) % m_max_size;
           m_array[m_back] = item;

           m_size++;

           m_cond.broadcast();
           m_mutex.unlock();
           return true;
       }
       //pop时，如果当前队列没有元素，将会等待条件变量
       bool pop( T &item )
       {
           m_mutex.lock();
           /* 多个消费者的时候这里要为while，而不是if */
           while( m_size <= 0 )
           {
               /* 
                    当重新抢到互斥锁，pthread_cond_wait返回为0，但是locker中写的返回ret==0 
                    意思是获取锁成功则return true，获取锁失败则return false;
               */
               if ( !m_cond.wait( m_mutex.get() ) )
                {
                    m_mutex.unlock();
                    return false;
                }
           }
           m_front = (m_front + 1) % m_max_size;
           item = m_array[m_front];
           m_size--;
           m_mutex.unlock();
           return true;
       }
       //增加了pop超时处理
       bool pop( T &item, int ms_timeout )
       {
           struct timespec t = {0, 0};
           struct timeval now = {0, 0};
           //Get the current time of day
           gettimeofday(&now, NULL);
           m_mutex.lock();
           if ( m_size <= 0 )
           {
               t.tv_sec = now.tv_sec + ms_timeout / 1000;
               t.tv_nsec = ( ms_timeout % 1000 ) * 1000;
               if (!m_cond.timewait(m_mutex.get(), t))
               {
                   m_mutex.unlock();
                   return false;
               }
           }
           if ( m_size <= 0 )
           {
               m_mutex.unlock();
               return false;
           }
           m_front = (m_front + 1) % m_max_size;
           item = m_array[m_front];
           m_size--;
           m_mutex.unlock();
           return true;
       }

    private:
        locker m_mutex;
        cond m_cond;
        T *m_array;
        int m_size;
        int m_max_size;
        int m_front;
        int m_back;
};

#endif // BLOCK_QUEUE_H