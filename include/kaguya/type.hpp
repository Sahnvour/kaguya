#pragma once

#include <string>
#include <cstring>

#include "kaguya/config.hpp"
#include "kaguya/traits.hpp"
#include "kaguya/exception.hpp"

namespace kaguya
{
	struct NewTable {
		NewTable() :reserve_array_(0), reserve_record_(0) {}
		NewTable(int reserve_array, int reserve_record_) :reserve_array_(reserve_array), reserve_record_(reserve_record_) {}
		int reserve_array_;
		int reserve_record_;
	};
	struct NewThread {};
	struct GlobalTable {};
	struct NilValue {};

	template<class T>
	struct MetaPointerWrapper {
		MetaPointerWrapper(T* p) :ptr(p) {}
		T* ptr;
	};

	namespace types
	{
		template<typename T>
		struct typetag {};




		namespace nodirectuse {
			typedef const std::string& metatableName_t;
			template<typename T>
			inline metatableName_t metatableNameNonCV(typetag<T>)
			{
				static const std::string v = typeid(T*).name() + std::string("_kaguya_type");
				return v;
			}
			template<typename T>
			inline metatableName_t metatableNameDispatch(typetag<T>)
			{
				typedef typename traits::remove_cv<T>::type noncv_type;
				typedef typename traits::remove_pointer<noncv_type>::type noncvpointer_type;
				typedef typename traits::remove_const_reference<noncvpointer_type>::type noncvpointerref_type;
				return metatableNameNonCV(typetag<noncvpointerref_type>());
			}
			template<typename T>
			inline metatableName_t metatableNameDispatch(typetag<MetaPointerWrapper<T> >)
			{
				typedef typename traits::remove_cv<T>::type noncv_type;
				typedef typename traits::remove_pointer<noncv_type>::type noncvpointer_type;
				typedef typename traits::remove_const_reference<noncvpointer_type>::type noncvpointerref_type;
				return metatableNameNonCV(typetag<MetaPointerWrapper<noncvpointerref_type> >());
			}
			template<typename T>
			inline metatableName_t metatableNameDispatch(typetag<standard::shared_ptr<T> >)
			{
				typedef typename traits::remove_cv<T>::type noncv_type;
				typedef typename traits::remove_pointer<noncv_type>::type noncvpointer_type;
				typedef typename traits::remove_const_reference<noncvpointer_type>::type noncvpointerref_type;
				return metatableNameNonCV(typetag<standard::shared_ptr<noncvpointerref_type> >());
			}
		}
		template<typename T>
		inline nodirectuse::metatableName_t metatableName()
		{
			typedef typename traits::remove_cv<T>::type noncv_type;
			typedef typename traits::remove_pointer<noncv_type>::type noncvpointer_type;
			typedef typename traits::remove_const_reference<noncvpointer_type>::type noncvpointerref_type;
			return nodirectuse::metatableNameDispatch(typetag<noncvpointerref_type>());
		}

		bool available_metatable(lua_State* l, const char* t)
		{
			util::ScopedSavedStack save(l);
			luaL_getmetatable(l, t);
			return LUA_TNIL != lua_type(l, -1);
		}
		template<typename T>
		bool available_metatable(lua_State* l, typetag<T> type = typetag<T>())
		{
			return available_metatable(l, metatableName<T>().c_str())
				|| available_metatable(l, metatableName<MetaPointerWrapper<T> >().c_str());
		}

		template<class T>
		T* get_pointer(lua_State* l, int index, typetag<T> tag)
		{
			int type = lua_type(l, index);

			if (type == LUA_TLIGHTUSERDATA)
			{
				return (T*)lua_topointer(l, index);
			}
			else if (type != LUA_TUSERDATA)
			{
				return 0;
			}
			else
			{
				T* ptr = (T*)luaL_testudata(l, index, metatableName<T>().c_str());
				if (!ptr)
				{
					MetaPointerWrapper<T>* ptr_wrapper = reinterpret_cast<MetaPointerWrapper<T>*>(luaL_testudata(l, index, metatableName<MetaPointerWrapper<T> >().c_str()));
					if (ptr_wrapper) { ptr = ptr_wrapper->ptr; }
				}
				if (!ptr)
				{
					standard::shared_ptr<T>* shared_ptr = reinterpret_cast<standard::shared_ptr<T>*>(luaL_testudata(l, index, metatableName<standard::shared_ptr<T> >().c_str()));
					if (shared_ptr) { ptr = shared_ptr->get(); }
				}

				return ptr;
			}
			return 0;
		}
		namespace detail
		{
			template<typename T>
			inline bool strictCheckType(lua_State* l, int index, typetag<T> tag)
			{
				return luaL_testudata(l, index, metatableName<T>().c_str()) != 0
					|| luaL_testudata(l, index, metatableName<standard::shared_ptr<typename traits::remove_const_reference<T>::type> >().c_str()) != 0
					|| luaL_testudata(l, index, metatableName<MetaPointerWrapper<typename traits::remove_const_reference<T>::type> >().c_str()) != 0;
			}

#if LUA_VERSION_NUM >= 503
			template<>
			inline bool strictCheckType(lua_State* l, int index, typetag<lua_Integer> tag)
			{
				return lua_isinteger(l, index) != 0;
			}
#endif

			template<>
			inline bool strictCheckType(lua_State* l, int index, typetag<lua_Number> tag)
			{
				return lua_type(l, index) == LUA_TNUMBER;
			}

			template<>
			inline bool strictCheckType(lua_State* l, int index, typetag<bool> tag)
			{
				return lua_type(l, index) == LUA_TBOOLEAN;
			}
			template<>
			inline bool strictCheckType(lua_State* l, int index, typetag<std::string> tag)
			{
				return lua_type(l, index) == LUA_TSTRING;
			}
			template<>
			inline bool strictCheckType(lua_State* l, int index, typetag<const char*> tag)
			{
				return lua_type(l, index) == LUA_TSTRING;
			}
			template<>
			inline bool strictCheckType(lua_State* l, int index, typetag<lua_State*> tag)
			{
				return lua_isthread(l, index);
			}


			template<typename T>
			inline bool checkType(lua_State* l, int index, typetag<T> tag)
			{
				return lua_type(l, index) == LUA_TLIGHTUSERDATA
					|| luaL_testudata(l, index, metatableName<T>().c_str()) != 0
					|| luaL_testudata(l, index, metatableName<standard::shared_ptr<typename traits::remove_const_reference<T>::type> >().c_str()) != 0
					|| luaL_testudata(l, index, metatableName<MetaPointerWrapper<typename traits::remove_const_reference<T>::type> >().c_str()) != 0;
			}

			template<typename T>
			inline bool checkType(lua_State* l, int index, typetag<T*> tag)
			{
				int type = lua_type(l, index);
				return type == LUA_TLIGHTUSERDATA
					|| (type == LUA_TNUMBER && lua_tonumber(l, index) == 0) //allow zero for nullptr
					|| luaL_testudata(l, index, metatableName<T>().c_str()) != 0
					|| luaL_testudata(l, index, metatableName<standard::shared_ptr<typename traits::remove_const_reference<T>::type> >().c_str()) != 0
					|| luaL_testudata(l, index, metatableName<MetaPointerWrapper<typename traits::remove_const_reference<T>::type> >().c_str()) != 0;
			}

			template<>
			inline bool checkType(lua_State* l, int index, typetag<bool> tag)
			{
				return true;
			}
#if LUA_VERSION_NUM >= 503
			template<>
			inline bool checkType(lua_State* l, int index, typetag<lua_Integer> tag)
			{
				return lua_isnumber(l, index) != 0;
			}
#endif
			template<>
			inline bool checkType(lua_State* l, int index, typetag<lua_Number> tag)
			{
				return lua_isnumber(l, index) != 0;
			}
			template<>
			inline bool checkType(lua_State* l, int index, typetag<std::string> tag)
			{
				return lua_isstring(l, index) != 0;
			}
			template<>
			inline bool checkType(lua_State* l, int index, typetag<const char*> tag)
			{
				return lua_isstring(l, index) != 0;
			}
			template<>
			inline bool checkType(lua_State* l, int index, typetag<lua_State*> tag)
			{
				return lua_isthread(l, index);
			}


			template<typename T>
			inline T get(lua_State* l, int index, typetag<T> tag = typetag<T>())
			{
				typename traits::remove_reference<T>::type* pointer = get_pointer(l, index, typetag<typename traits::remove_reference<T>::type>());
				if (!pointer)
				{
					throw LuaTypeMismatch("type mismatch!!");
				}
				return *pointer;
			}
#if LUA_VERSION_NUM >= 503
			template<typename T>
			inline T get(lua_State* l, int index, typetag<typename traits::enum_dispatch_type<T> > tag)
			{
				return T(lua_tointeger(l, index));
			}
#else
			template<typename T>
			inline T get(lua_State* l, int index, typetag<typename traits::enum_dispatch_type<T> > tag)
			{
				return T(lua_tonumber(l, index));
			}
#endif
			template<typename T>
			inline T* get(lua_State* l, int index, typetag<T*> tag = typetag<T*>())
			{
				return get_pointer(l, index, typetag<T>());
			}
			template<>
			inline lua_State* get(lua_State* l, int index, typetag<lua_State*> tag)
			{
				return lua_tothread(l, index);
			}
			template<>
			inline bool get(lua_State* l, int index, typetag<bool> tag)
			{
				return lua_toboolean(l, index) != 0;
			}
			template<>
			inline lua_Number get(lua_State* l, int index, typetag<lua_Number> tag)
			{
				return lua_tonumber(l, index);
			}
#if LUA_VERSION_NUM >= 503
			template<>
			inline lua_Integer get(lua_State* l, int index, typetag<lua_Integer> tag)
			{
				return lua_tointeger(l, index);
			}
#endif

			template<>
			inline std::string get(lua_State* l, int index, typetag<std::string> tag)
			{
				size_t size = 0;
				const char* buffer = lua_tolstring(l, index, &size);
				return std::string(buffer, size);
			}
			template<>
			inline const char* get(lua_State* l, int index, typetag<const char*> tag)
			{
				return lua_tostring(l, index);
			}


			template<typename T>
			inline int push(lua_State* l,T& v)
			{
				if (!available_metatable<T>(l))
				{
					lua_pushlightuserdata(l, &v);
				}
				else
				{
					typedef MetaPointerWrapper<T> wrapper_type;
					void *storage = lua_newuserdata(l, sizeof(wrapper_type));
					new(storage) wrapper_type(&v);
					luaL_setmetatable(l, metatableName<wrapper_type>().c_str());
				}
				return 1;
			}
			inline int push(lua_State* l, bool v)
			{
				lua_pushboolean(l, v);
				return 1;
			}
			inline int push(lua_State* l, lua_Number v)
			{
				lua_pushnumber(l, v);
				return 1;
			}
#if LUA_VERSION_NUM >= 503
			inline int push(lua_State* l, lua_Integer v)
			{
				lua_pushinteger(l, v);
				return 1;
			}
#endif
			inline int push(lua_State* l, const std::string& s)
			{
				lua_pushlstring(l, s.c_str(), s.size());
				return 1;
			}
			inline int push(lua_State* l, const char* s)
			{
				lua_pushstring(l, s);
				return 1;
			}
			template<typename T>
			inline int push(lua_State* l, const T& v)
			{
				void *storage = lua_newuserdata(l, sizeof(T));
				new(storage) T(v);
				luaL_setmetatable(l, types::metatableName<T>().c_str());
				return 1;
			}

			template<typename T>
			inline int push(lua_State* l, T* v)
			{
				if (!v)
				{
					lua_pushnil(l);
				}
				else if (!available_metatable<T>(l))
				{
					lua_pushlightuserdata(l, v);
				}
				else
				{
					typedef MetaPointerWrapper<T> wrapper_type;
					void *storage = lua_newuserdata(l, sizeof(wrapper_type));
					new(storage) wrapper_type(v);
					luaL_setmetatable(l, metatableName<wrapper_type>().c_str());
				}
				return 1;
			}
			template<typename T>
			inline int push(lua_State* l, const T* v)
			{
				if (!v)
				{
					lua_pushnil(l);
				}
				else if (!available_metatable<T>(l))
				{
					lua_pushlightuserdata(l, const_cast<T*>(v));
				}
				else
				{
					typedef MetaPointerWrapper<T> wrapper_type;
					void *storage = lua_newuserdata(l, sizeof(wrapper_type));
					new(storage) wrapper_type(v);
					luaL_setmetatable(l, metatableName<wrapper_type>().c_str());
				}
				return 1;
			}
		}
		template<typename T>
		inline bool strictCheckType(lua_State* l, int index, typetag<T> tag)
		{
			return detail::strictCheckType(l, index, typetag<typename traits::lua_push_type<T>::type>());
		}
		template<typename T>
		inline bool checkType(lua_State* l, int index, typetag<T> tag)
		{
			return detail::checkType(l, index, typetag<typename traits::lua_push_type<T>::type>());
		}

		template<typename T>
		inline typename traits::arg_get_type<T>::type get(lua_State* l, int index, typetag<T> tag)
		{
			return detail::get(l, index, typetag<typename traits::arg_get_type_dispatch<T>::type>());
		}

		template<typename T>
		inline int push(lua_State* l,const T& v)
		{
			detail::push(l, static_cast<typename traits::lua_push_type<T>::type>(v));
			return 1;
		}

		template<>
		inline int push(lua_State* l, const NewTable& table)
		{
			lua_createtable(l, table.reserve_array_, table.reserve_record_);
			return 1;
		}

		template<>
		inline int push(lua_State* l, const NewThread&)
		{
			lua_newthread(l);
			return 1;
		}

		template<>
		inline int push(lua_State* l, const NilValue&)
		{
			lua_pushnil(l);
			return 1;
		}

		template<>
		inline int push(lua_State* l, const  GlobalTable&)
		{
#if LUA_VERSION_NUM >= 502
			lua_pushglobaltable(l);
#else
			lua_pushvalue(l, LUA_GLOBALSINDEX);
#endif
			return 1;
		}


		//vector
		template<typename T>
		inline bool strictCheckType(lua_State* l, int index, typetag<std::vector<T> >);
		template<typename T>
		inline bool checkType(lua_State* l, int index, typetag<std::vector<T> >);
		template<typename T>
		inline std::vector<T> get(lua_State* l, int index, typetag<std::vector<T> > tag);
		template<typename T>
		inline int push(lua_State* l, const std::vector<T>& ref);
		//std::map
		template<typename K, typename V>
		inline bool strictCheckType(lua_State* l, int index, typetag<std::map<K, V> >);
		template<typename K, typename V>
		inline bool checkType(lua_State* l, int index, typetag<std::map<K, V> >);
		template<typename K, typename V>
		inline std::map<K, V> get(lua_State* l, int index, typetag<std::map<K, V> > tag);
		template<typename K, typename V>
		inline int push(lua_State* l, const std::map<K, V>& ref);
#include "kaguya/gen/push_tuple.inl"


#include "kaguya/gen/constructor.inl"

		template<typename T>inline void destructor(T* pointer)
		{
			if (pointer)
			{
				pointer->~T();
			}
		}
	}

};
