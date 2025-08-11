#include<mutex>
#include<thread>
#include<condition_variable>
#include<queue>

/*
���ͷ�ļ���Ҫʵ��һϵ�дֿ����ȵ��̰߳�ȫ���У�Ϊ����
�Ķ��̱߳���ṩ������ʩ��
*/


/*
�ֿ����ȵ��̰߳�ȫ���У�����ȫ�ֻ���������������ʵ��
*/

template<typename T>
class ThreadSafeQueue
{
private:
    std::queue<T> queue_; // �洢���ݵĶ���
    mutable std::mutex mutex_; // ����������������
    std::condition_variable cond_var_; // ��������������֪ͨ�ȴ��߳�
public:
    //�������������
    ThreadSafeQueue() = default;
    ~ThreadSafeQueue() = default;

    // ��ֹ��������͸�ֵ
    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

    //�����ƶ�����͸�ֵ
    ThreadSafeQueue(ThreadSafeQueue&& other) noexcept
    {
        std::lock_guard<std::mutex> lock(other.mutex_);
        queue_ = std::move(other.queue_);
    }
    ThreadSafeQueue& operator=(ThreadSafeQueue&& other) noexcept
    {
        if (this != &other) {
            std::lock_guard<std::mutex> lock1(mutex_);
            std::lock_guard<std::mutex> lock2(other.mutex_);
            queue_ = std::move(other.queue_);
        }
        return *this;
    }

    //������Ա����
    // ���Ԫ�ص�����
    void push(T value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(value));
        cond_var_.notify_one(); // ֪ͨһ���ȴ��߳�
    }

    // �Ӷ����л�ȡԪ��
    bool try_pop(T& value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false; // ����Ϊ��
        }
        value = std::move(queue_.front());
        queue_.pop();
        return true; // �ɹ���ȡԪ��
    }

    std::shared_ptr<T> try_pop()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return nullptr; // ����Ϊ��
        }
        auto value = std::make_shared<T>(std::move(queue_.front()));
        queue_.pop();
        return value; // �ɹ���ȡԪ��
    }

    // �����ػ�ȡԪ�أ��������Ϊ����ȴ�
    bool wait_and_pop(T& value)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_var_.wait(lock, [this] { return !queue_.empty(); }); // �ȴ�ֱ�����в�Ϊ��
        value = std::move(queue_.front());
        queue_.pop();
        return true; // �ɹ���ȡԪ��
    }

    std::shared_ptr<T> wait_and_pop()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_var_.wait(lock, [this] { return !queue_.empty(); }); // �ȴ�ֱ�����в�Ϊ��
        auto value = std::make_shared<T>(std::move(queue_.front()));
        queue_.pop();
        return value; // �ɹ���ȡԪ��
    }

    // �������Ƿ�Ϊ��
    bool empty() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty(); // ���ض����Ƿ�Ϊ��
    }

    // ��ȡ���еĴ�С
    std::size_t size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size(); // ���ض��еĴ�С
    }
};
