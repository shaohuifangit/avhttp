//
// cookie.hpp
// ~~~~~~~~~~
//
// Copyright (c) 2013 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// path LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef AVHTTP_COOKIE_HPP
#define AVHTTP_COOKIE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <stdlib.h>     // for atol.
#include <string>
#include <vector>
#include <algorithm>
#include <iterator>
#include <boost/date_time.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>

#include "avhttp/detail/escape_string.hpp"
#include "avhttp/detail/parsers.hpp"
#include "avhttp/file.hpp"

namespace avhttp {

class cookies;
inline cookies operator+(const cookies& lhs, const cookies& rhs);

// 用于管理cookie的实现.
// 请求前设置cookie, 示例如下:.
// @begin example
//  http_stream h(io);
//  ...
// 	cookies cookie;
// 	cookie("cookie_name1", "hohoo");
// 	cookie("cookie_name1", "hahaha");
//  设置到http_stream.
// 	h.http_cookies(cookie);
// @end example
// 需要访问http服务器返回的cookie, 示例如下:
// @begin example
//  http_stream h(io);
//  ...
//  cookies cookie = h.http_cookies();
//  cookies::iterator i = cookie.begin();
//  for (; i != cookie.end(); i++)
//  {
//    std::cout <<　i->name << ":" << i->value << std::endl;
//  }
// @end example
class cookies
{
public:
	// cookie结构定义.
	typedef struct cookie_t
	{
		cookie_t()
			: httponly(false)
			, secure(false)
		{}

		// name和value, name必须不为空串.
		std::string name;
		std::string value;

		// 如果为空串, 即匹配任何域名.
		std::string domain;

		// 如果为空串, 即匹配任何路径.
		std::string path;

		// 如果为无效时间, 表示为不过期.
		boost::posix_time::ptime expires;

		// httponly在这里没有意义, secure只有为https时, 才带上此cookie.
		bool httponly;
		bool secure;

		// 用于 std::sort 算法的比较函数对象.
		// 基于 过期时间进行排序.
		template<typename Compare>
		struct compare_by_expires_t
			: std::binary_function<cookie_t, cookie_t, bool>
		{
			compare_by_expires_t(const Compare& comp)
			  : m_comp(comp)
			{
			}

			bool operator()(const cookie_t& lhs, const cookie_t& rhs)
			{
				return m_comp(lhs.expires, rhs.expires);
			}

        private:
			Compare m_comp;
		};

		template<typename Compare>
		static compare_by_expires_t<Compare>
		compare_by_expires(const Compare & comp)
		{
			return compare_by_expires_t<Compare>(comp);
		}

	} http_cookie;

public:

	/// Constructor.
	explicit cookies()
	{}

	/// Destructor.
	~cookies()
	{}

	typedef std::vector<http_cookie>::iterator iterator;
	typedef std::vector<http_cookie>::const_iterator const_iterator;
	typedef std::vector<http_cookie>::reference reference;
	typedef std::vector<http_cookie>::const_reference const_reference;
	typedef std::vector<http_cookie>::value_type value_type;

	iterator begin() { return m_cookies.begin(); }
	iterator end() { return m_cookies.end(); }

	const_iterator begin() const { return m_cookies.begin(); }
	const_iterator end() const { return m_cookies.end(); }

	void push_back(const value_type& val) { m_cookies.push_back(val); }

	// 保存cookies到指定的文件, 以Netscape HTTP Cookie File格式保存, 兼容curl.
	// @param filename指定保存的文件名.
	// @param default_domain默认的domain, 如果http服务器没有返回domain, 保存时
	//   将以default_domain作为domain来处理, default_domain一般可以指定为所访问
	//   的http服务器domain.
	// 备注: 失败将抛出一个boost::system::system_error异常.
	void save_to_file(const std::string& filename, const std::string& default_domain = "") const
	{
		std::fstream f;
		f.open(filename.c_str(), std::ios::out);
		if (!f.is_open())
		{
			f.open(filename.c_str(), std::ios::out|std::ios::trunc);
			if (!f.is_open())
			{
				boost::system::error_code ec =
					make_error_code(boost::system::errc::no_such_file_or_directory);
				boost::throw_exception(boost::system::system_error(ec));
			}
		}

		// 写入memo信息.
		if (fs::file_size(filename) == 0)
		{
			std::string memo =
				"# Netscape HTTP Cookie File\n"
				"# http://curl.haxx.se/docs/http-cookies.html\n"
				"# This file was generated by libcurl! Edit at your own risk.\n\n";
			f.write(memo.c_str(), memo.size());
		}

		std::vector<http_cookie>::const_iterator i = m_cookies.begin();
		for (; i != m_cookies.end(); i++)
		{
			const http_cookie& cookie = *i;
			std::string tmp;

			// domain.
			if (cookie.domain.empty())
			{
				tmp = default_domain;
			}
			else
			{
				tmp = cookie.domain;
			}
			f.write(tmp.c_str(), tmp.size());
			f.write("\t", 1);

			// flag.
			cookie.domain.empty() ? tmp = "FALSE" : tmp = "TRUE";
			f.write(tmp.c_str(), tmp.size());
			f.write("\t", 1);

			// path.
			f.write(cookie.path.c_str(), cookie.path.size());
			f.write("\t", 1);

			// secure.
			cookie.secure ? tmp = "TRUE" : tmp = "FALSE";
			f.write(tmp.c_str(), tmp.size());
			f.write("\t", 1);

			// expires.
			time_t t = 0;
			if (cookie.expires != boost::posix_time::not_a_date_time)
			{
				t = detail::ptime_to_time_t(cookie.expires);
			}
			std::ostringstream oss;
			oss.imbue(std::locale("C"));
			oss << t;
			tmp = oss.str();
			f.write(tmp.c_str(), tmp.size());
			f.write("\t", 1);

			// name.
			f.write(cookie.name.c_str(), cookie.name.size());
			f.write("\t", 1);

			// value.
			f.write(cookie.value.c_str(), cookie.value.size());
			f.write("\t\n", 1);
		}
	}

	// 从文件加载cookie.
	// @param filename指定的文件名, 该文件必须是Netscape HTTP Cookie File格式保存.
	// 备注: 失败将抛出一个boost::system::system_error异常.
	void load_from_file(const std::string& filename)
	{
		std::fstream f;
		f.open(filename.c_str(), std::ios::in|std::ios::out);
		if (!f.is_open())
		{
			boost::system::error_code ec =
				make_error_code(boost::system::errc::no_such_file_or_directory);
			boost::throw_exception(boost::system::system_error(ec));
		}

		std::string line;
		while (!f.eof())
		{
			std::getline(f, line);
			// 过滤掉空行和注释行.
			boost::trim(line);
			if (line.empty() || line.at(0) == '#')
			{
				continue;
			}

			// 开始解析.
			std::vector<std::string> split_vec;
			boost::split(split_vec, line, boost::is_any_of("\t"), boost::token_compress_on);
			time_t time;
			time = atol(split_vec[4].c_str());
			http_cookie cookie;
			cookie.domain = split_vec[0];
			cookie.path = split_vec[2];
			cookie.secure = split_vec[3] == "TRUE" ? true : false;
			cookie.expires = boost::posix_time::from_time_t(time);
			std::string s = boost::posix_time::to_simple_string(cookie.expires);
			cookie.name = split_vec[5];
			cookie.value = split_vec[6];

			// 更新或插入cookie.
			m_cookies.push_back(cookie);
		}
	}

	// 更新一个cookie.
	// @name 指定cookie的名称.
	// @value cookie的值.
	cookies& operator()(const std::string& name, const std::string& value)
	{
		http_cookie tmp;
		tmp.name = name;
		tmp.value = value;
		m_cookies.push_back(tmp);
		return *this;
	}

	// 更新一个cookie.
	// @cookie 指定cookie
	cookies& operator()(const http_cookie & cookie)
	{
		m_cookies.push_back(cookie);
		return *this;
	}

	// 更新cookie, 接受一个http服务器返回的Set-Cookie格式字符串的cookie.
	cookies& operator()(const std::string& str)
	{
		std::vector<http_cookie> cookie;
		if (parse_cookie_string(str, cookie))
		{
			for (std::vector<http_cookie>::iterator i = cookie.begin();
				i != cookie.end(); i++)
			{
				m_cookies.push_back(*i);
			}
		}

		return *this;
	}

	// 获取HTTP请求头需要的 cookie 行
	// @is_https 是否为 https 连接.
	std::string get_cookie_line(bool is_https = false) const
	{
		std::string cookie;
		if (m_cookies.size() != 0)
		{
			cookies tmp;

			// 通过+运算自动去掉重复的cookie项.
			tmp = tmp + *this;

			// 构造字符串.
			cookies::const_iterator i = tmp.begin();
			for (; i != tmp.end(); i++)
			{
				// 判断是否为空.
				if (i->value.empty())
					continue;
				// 判断secure.
				if (i->secure && !is_https)
					continue;
				// 判断是否过期, 小于当前时间表示过期, 不添加到cookie.
				if ( !i->expires.is_not_a_date_time()
					&& i->expires < boost::posix_time::second_clock::local_time())
				{
					continue;
				}

				if (!cookie.empty())
				{
					cookie += "; ";
				}

				cookie += (i->name + "=" + i->value);
			}
		}
		return cookie;
	}

	// 获取指定的 cookie
	std::string operator[](const std::string& key) const
	{
		const_iterator i = m_cookies.begin();

		for (; i != m_cookies.end(); ++i)
		{
			if (i->name == key && !i->value.empty())
				return i->value;
		}

		return "";
	}

	// 查找基于 名字.
	iterator find(const std::string& key)
	{
		iterator i = m_cookies.begin();

		for (; i != m_cookies.end(); ++i)
		{
			if (i->name == key)
				return i;
		}

		return end();
	}

	// 查找基于 名字.
	const_iterator find(const std::string& key) const
	{
		const_iterator i = m_cookies.begin();

		for (; i != m_cookies.end(); ++i)
		{
			if (i->name == key)
				return i;
		}

		return end();
	}

	// 查找基于 域/路径/名字.
	iterator find(const http_cookie& key)
	{
		iterator i = m_cookies.begin();

		for (; i != m_cookies.end(); ++i)
		{
			if (i->name == key.name && i->domain == key.domain && i->path == key.path)
				return i;
		}

		return end();
	}

	// 查找基于 域/路径/名字.
	const_iterator find(const http_cookie& key) const
	{
		const_iterator i = m_cookies.begin();

		for (; i != m_cookies.end(); ++i)
		{
			if (i->name == key.name && i->domain == key.domain && i->path == key.path)
				return i;
		}

		return end();
	}

private:
	struct http_cookie_is_same_name
	{
		http_cookie_is_same_name(const std::string & name)
		  : m_name(name)
		{}

		bool operator()( const http_cookie & cookie)
		{
			return cookie.name == m_name ;
		}

		std::string m_name;
	};

public:
	// 删除指定名称的cookie.
	void remove_cookie(const std::string& name)
	{
		std::remove_if(m_cookies.begin(), m_cookies.end(),
			http_cookie_is_same_name(name) );
	}

	// 清除所有cookie.
	void clear()
	{
		m_cookies.clear();
	}

	// 返回cookie数.
	int size() const
	{
		return m_cookies.size();
	}

	// 保留n个cookie空间.
	void reserve(std::size_t n)
	{
		m_cookies.reserve(n);
	}

	// 设置默认domain.
	void default_domain(const std::string& domain)
	{
		m_default_domain = domain;
	}

	// 返回当前默认domain.
	std::string default_domain() const
	{
		return m_default_domain;
	}

private:

	// 解析cookie字符.
	// 示例字符串:
	// gsid=none; expires=Sun, 22-Sep-2013 14:27:43 GMT; path=/; domain=.fidelity.cn; httponly
	// gsid=none; gsid2=none; expires=Sun, 22-Sep-2013 14:27:43 GMT; path=/; domain=.fidelity.cn
	inline bool parse_cookie_string(const std::string& str, std::vector<http_cookie>& cookie)
	{
		// 解析状态.
		enum
		{
			cookie_name_start,
			cookie_name,
			cookie_value_start,
			cookie_value,
			cookie_bad
		} state = cookie_name_start;

		std::string::const_iterator iter = str.begin();
		std::string name;
		std::string value;
		std::map<std::string, std::string> tmp;
		http_cookie cookie_tmp;

		// 开始解析http-cookie字符串.
		while (iter != str.end() && state != cookie_bad)
		{
			const char c = *iter++;
			switch (state)
			{
			case cookie_name_start:
				if (c == ' ')
					continue;
				if (detail::is_char(c))
				{
					name.push_back(c);
					state = cookie_name;
				}
				else
					state = cookie_bad;
				break;
			case cookie_name:
				if (c == ';')
				{
					state = cookie_name_start;
					if (name == "secure")
						cookie_tmp.secure = true;
					else if (name == "httponly")
						cookie_tmp.httponly = true;
					else
						state = cookie_bad;
					name = "";
				}
				else if (c == '=')
				{
					value = "";
					state = cookie_value_start;
				}
				else if (detail::is_tspecial(c) || c == ':')
				{
					name = "";
					state = cookie_name_start;
				}
				else if (detail::is_char(c) || c == '_')
					name.push_back(c);
				break;
			case cookie_value_start:
				if (c == ';')
				{
					tmp[name] = value; // 添加或更新.
					name = "";
					value = "";
					state = cookie_name_start;
					continue;
				}

				if (c == '\"' || c == '\'')
					continue;

				if (detail::is_char(c))
				{
					value.push_back(c);
					state = cookie_value;
				}
				else
					state = cookie_bad;
				break;
			case cookie_value:
				if (c == ';' || c == '\"' || c == '\'')
				{
					tmp[name] = value;	// 添加或更新.
					name = "";
					value = "";
					state = cookie_name_start;
				}
				else if (detail::is_char(c))
					value.push_back(c);
				else
					state = cookie_bad;
				break;
			case cookie_bad:
				break;
			}
		}
		if (state == cookie_name && !name.empty())
		{
			if (name == "secure")
				cookie_tmp.secure = true;
			else if (name == "httponly")
				cookie_tmp.httponly = true;
		}
		else if (state == cookie_value && !value.empty())
		{
			tmp[name] = value;	// 添加或更新.
		}
		else if (state == cookie_bad)
		{
			return false;
		}

		for (std::map<std::string, std::string>::iterator i = tmp.begin();
			i != tmp.end(); )
		{
			if (boost::to_lower_copy(i->first) == "expires")
			{
				if (!detail::parse_http_date(i->second, cookie_tmp.expires))
					BOOST_ASSERT(0);	// for debug.
				tmp.erase(i++);
			}
			else if (boost::to_lower_copy(i->first) == "domain")
			{
				cookie_tmp.domain = i->second;
				if (i->second.empty() && !m_default_domain.empty())
				{
					cookie_tmp.domain = m_default_domain;
				}
				tmp.erase(i++);
			}
			else if (boost::to_lower_copy(i->first) == "path")
			{
				cookie_tmp.path = i->second;
				tmp.erase(i++);
			}
			else
			{
				i++;
			}
		}

		// 添加到容器返回.
		for (std::map<std::string, std::string>::iterator i = tmp.begin();
			i != tmp.end(); i++)
		{
			cookie_tmp.name = i->first;
			cookie_tmp.value = i->second;
			cookie.push_back(cookie_tmp);
		}

		return true;
	}

private:
	// 保存所有cookie.
	std::vector<http_cookie> m_cookies;

	// 设置默认域.
	std::string m_default_domain;
};

namespace detail {

inline bool cookie_megerable(const cookies::http_cookie& element, const cookies& container)
{
	// 查看是否 element 符合拷贝进 container 的资格.
	// 第一，他应该不是空的.
	cookies::const_iterator it = container.find(element.name);

	// 首先，它不存在, 那就直接更新.
	if (container.find(element) == container.end())
	{
		return true;
	}

	// 既然存在了，那就得比较比较.

	// 要拷贝进去的自己都是空的，有什么资格要拷贝进去!
	if (element.value.empty())
		return false;

	// 不过如果原来的是空的，那就可以拷贝进去.
	if (it->value.empty())
		return true;

	// 要是原来的时间要比你新，那还是不能拷贝.
	if (element.expires <= it->expires)
		return false;

	// 好了，基本上不能拷贝的都判定好了，接下来就是能拷贝的了.
	return true;
}

inline bool cookie_not_megerable(const cookies::http_cookie& element, const cookies& container)
{
	return !cookie_megerable(element, container);
}

} // namespace detail

// 将两个 cookies 容器合并.
inline cookies operator+(const cookies& lhs, const cookies& rhs)
{
	cookies tmp, ret;

	// 首先一股脑的拷贝进去.
	std::copy(lhs.begin(), lhs.end(), std::back_inserter(tmp));
	std::copy(rhs.begin(), rhs.end(), std::back_inserter(tmp));

	// 排序！ 我是多么希望能使用 c++11 的 lambda 表达式来简化这里的代码!
	std::sort(tmp.begin(), tmp.end(),
		cookies::http_cookie::compare_by_expires(std::greater<boost::posix_time::ptime>()));

	// NOTE: 本来是使用 std::copy_if 的，这样就可以使用 cookie_megerable 作为比较
	// 但是 c++98 里没有 std::copy_if, 因此使用了 remove_copy_if ,  这样就要把
	// cookie_megerable 含义反过来，故而有此函数.
	std::remove_copy_if(tmp.begin(), tmp.end(), std::back_inserter(ret),
		boost::bind(detail::cookie_not_megerable, _1, boost::ref(ret)));
	// 返回完成结果.
	return ret;
}

} // namespace avhttp

#endif // AVHTTP_COOKIE_HPP
