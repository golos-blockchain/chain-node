
namespace golos { namespace api {

template<typename arg>
struct callback_info {
    using callback_t = std::function<void(arg)>;
    using ptr = std::shared_ptr<callback_info>;
    using cont = std::list<ptr>;

    callback_t callback;
    boost::signals2::connection connection;
    typename cont::iterator it;

    void connect(
        boost::signals2::signal<void(arg)>& sig,
        cont& free_cont,
        callback_t cb
    ) {
        callback = cb;
        connection = sig.connect([this, &free_cont](arg item) {
            wlog("V3");
            try {
                this->callback(item);
            } catch (...) {
                free_cont.push_back(*this->it);
                this->connection.disconnect();
            }
        });
    }
};

} } // golos::api
