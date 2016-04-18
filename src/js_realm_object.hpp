////////////////////////////////////////////////////////////////////////////
//
// Copyright 2016 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include "js_class.hpp"
#include "js_types.hpp"
#include "js_util.hpp"

#include "object_accessor.hpp"
#include "object_store.hpp"

namespace realm {
namespace js {

template<typename T>
class RealmObject {
    using TContext = typename T::Context;
    using TFunction = typename T::Function;
    using TObject = typename T::Object;
    using TValue = typename T::Value;
    using String = String<T>;
    using Value = Value<T>;
    using Object = Object<T>;
    using Function = Function<T>;
    using ReturnValue = ReturnValue<T>;

  public:
    static TObject create_instance(TContext, realm::Object &);

    static void get_property(TContext, TObject, const String &, ReturnValue &);
    static bool set_property(TContext, TObject, const String &, TValue);
    static std::vector<String> get_property_names(TContext, TObject);
};

template<typename T>
struct RealmObjectClass : ClassDefinition<T, realm::Object> {
    using RealmObject = RealmObject<T>;

    const std::string name = "RealmObject";

    const StringPropertyType<T> string_accessor = {
        wrap<RealmObject::get_property>,
        wrap<RealmObject::set_property>,
        wrap<RealmObject::get_property_names>,
    };
};

template<typename T>
typename T::Object RealmObject<T>::create_instance(TContext ctx, realm::Object &realm_object) {
    static String prototype_string = "prototype";

    auto delegate = get_delegate<T>(realm_object.realm().get());
    auto name = realm_object.get_object_schema().name;
    auto object = create_object<T, RealmObjectClass<T>>(ctx, new realm::Object(realm_object));

    if (!delegate->m_constructors.count(name)) {
        return object;
    }

    TFunction constructor = delegate->m_constructors.at(name);
    TObject prototype = Object::validated_get_object(ctx, constructor, prototype_string);
    Object::set_prototype(ctx, object, prototype);

    TValue result = Function::call(ctx, constructor, object, 0, NULL);
    if (result != object && !Value::is_null(ctx, result) && !Value::is_undefined(ctx, result)) {
        throw std::runtime_error("Realm object constructor must not return another value");
    }
    
    return object;
}

template<typename T>
void RealmObject<T>::get_property(TContext ctx, TObject object, const String &property, ReturnValue &return_value) {
    try {
        auto realm_object = get_internal<T, RealmObjectClass<T>>(object);
        auto result = realm_object->template get_property_value<TValue>(ctx, property);
        return_value.set(result);
    } catch (InvalidPropertyException &ex) {
        // getters for nonexistent properties in JS should always return undefined
    }
}

template<typename T>
bool RealmObject<T>::set_property(TContext ctx, TObject object, const String &property, TValue value) {
    auto realm_object = get_internal<T, RealmObjectClass<T>>(object);
    realm_object->set_property_value(ctx, property, value, true);
    return true;
}

template<typename T>
std::vector<String<T>> RealmObject<T>::get_property_names(TContext ctx, TObject object) {
    auto realm_object = get_internal<T, RealmObjectClass<T>>(object);
    auto &properties = realm_object->get_object_schema().properties;

    std::vector<String> names;
    names.reserve(properties.size());

    for (auto &prop : properties) {
        names.push_back(prop.name);
    }

    return names;
}

} // js
} // realm