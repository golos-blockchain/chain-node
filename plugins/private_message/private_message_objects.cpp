#include <golos/plugins/private_message/private_message_objects.hpp>

namespace golos { namespace plugins { namespace private_message {
    bool starts_with_dog(const std::string& str) {
        return _NAME_HAS_DOG(str);
    }

    bool trim_dog(std::string& str) {
        if (_NAME_HAS_DOG(str)) {
            str.erase(0, 1);
            return true;
        }
        return false;
    }

    bool prepend_dog(std::string& str) {
        if (!_NAME_HAS_DOG(str)) {
            str.insert(0, "@");
            return true;
        }
        return false;
    }

    std::string without_dog(const std::string& str) {
        std::string new_str(str);
        trim_dog(new_str);
        return new_str;
    }

    std::string with_dog(const std::string& str) {
        std::string new_str(str);
        prepend_dog(new_str);
        return new_str;
    }
}}} // golos::plugins::private_message