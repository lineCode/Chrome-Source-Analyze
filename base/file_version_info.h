
#ifndef __file_version_info_h__
#define __file_version_info_h__

#include <windows.h>

#include <string>

namespace base
{
    class FilePath;
}

// 方便的获取文件的版本信息.
class FileVersionInfo
{
public:
    ~FileVersionInfo();

    // 创建指定路径文件的FileVersionInfo对象. 错误(通常是不存在或者无法打开)返回
    // NULL. 返回的对象使用完之后需要删除.
    static FileVersionInfo* CreateFileVersionInfo(const base::FilePath& file_path);

    // 创建当前模块的FileVersionInfo对象. 错误返回NULL. 返回的对象使用完之后需要
    // 删除.
    static FileVersionInfo* CreateFileVersionInfoForCurrentModule();

    // 获取版本信息的各种属性, 不存在返回空.
    virtual std::wstring company_name();
    virtual std::wstring company_short_name();
    virtual std::wstring product_name();
    virtual std::wstring product_short_name();
    virtual std::wstring internal_name();
    virtual std::wstring product_version();
    virtual std::wstring private_build();
    virtual std::wstring special_build();
    virtual std::wstring comments();
    virtual std::wstring original_filename();
    virtual std::wstring file_description();
    virtual std::wstring file_version();
    virtual std::wstring legal_copyright();
    virtual std::wstring legal_trademarks();
    virtual std::wstring last_change();
    virtual bool is_official_build();

    // 获取其它属性, 不限于上面的.
    bool GetValue(const wchar_t* name, std::wstring& value);

    // 和GetValue一样, 只是不存在的时候返回空字符串.
    std::wstring GetStringValue(const wchar_t* name);

    // 获取固定的版本信息, 如果不存在返回NULL.
    VS_FIXEDFILEINFO* fixed_file_info() { return fixed_file_info_; }

private:
    FileVersionInfo(void* data, int language, int code_page);

    FileVersionInfo(const FileVersionInfo&);
    void operator=(const FileVersionInfo&);

    void* data_;
    int language_;
    int code_page_;
    // 指向data_内部的指针, NULL表示不存在.
    VS_FIXEDFILEINFO* fixed_file_info_;
};

#endif //__file_version_info_h__