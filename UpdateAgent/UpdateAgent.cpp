// UpdateAgent.cpp : 定义应用程序的入口点。
//

#include "stdafx.h"
#include <map>
#include "UpdateAgent.h"
#include "RunCmdFilter.h"
#include "AppModule.h"
#include "HttpClient.h"
#include "UpdateSchedule.h"
#include "List.h"

CString cmdSelfKey() {
	CString chBuf;
	chBuf.Format(_T("entryName:%s"), svy::GetAppName());
	return chBuf;
}

//前置逻辑
class PreLogic
{
public:
	PreLogic() {
		bRunUpdate = false;
	}
	void parseCommondLine() {
		RunCmdFilter argsFilter;
		argsFilter.addFilter(std::bind(&PreLogic::handler,this, std::placeholders::_1, std::placeholders::_2));
		argsFilter.runParser();
		//
		svy::SinglePtr<AppModule> app;
		UINT nMax = mExes_.size();
		for (UINT nI = 0; nI < nMax; nI++) {
			ExeModule m = mExes_[nI];
			if (m.mVer_.mEntryName_.CompareNoCase(svy::GetAppName())) {
				if( !m.getPathFile().IsEmpty() && m.mHandle_ &&
					!m.getPrivateCA().IsEmpty() && !m.getPublicCA().IsEmpty() )
					app->addModule(m);
			}
			else
				app->addModule(m);
		}
	}
	bool canRunUpdate() {
		return bRunUpdate;
	}
protected:
	void handler(const CString& a, const CString& b){
		if (a == '|') {
			ExeModule exe;
			exe.mVer_.mVer_ = margs_[_T("version")];
			exe.mVer_.mProductCode_ = margs_[_T("productID")];
			exe.mVer_.mEntryName_ = margs_[_T("entryName")];
			if (0 == exe.mVer_.mEntryName_.CompareNoCase(svy::GetAppName())) {
				AppModule::REG_INFO regInfo;
				AppModule::ReadRegisteInfo(regInfo);
				CString path = svy::catUrl(regInfo.path, svy::GetAppName());			
				path += _T(".exe");
				exe.setExePath(path);
				bRunUpdate = true;
			}
			else {
				//自己运行完后退出因此pid没用
				exe.setPid(margs_[_T("pid")]);
			}
			exe.setPublicCA(margs_[_T("pubCA")]);
			exe.setPrivateCA(margs_[_T("priCA")]);
			//临时缓存
			mExes_.push_back(exe);
			margs_.clear();
		}
		else
			margs_.insert(std::make_pair(a, b));
	}
private:
	typedef std::map<CString, CString>	Args;
	ExeModules mExes_;
	Args	margs_;
	bool	bRunUpdate;
};

void testLua() {
	svy::SinglePtr<AppModule> app;
	lua_State *L = app->getLua();
	CString luaMain = _T("maintain.lua");
	luaL_dofile(L, CT2CA(luaMain));
	lua_getglobal(L, "BeginUpdate");
	lua_pushstring(L, "12121212");
	lua_pushstring(L, "test something1");
	lua_pushstring(L, "test something2");
	lua_pcall(L, 3, 0, 0);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
#if defined(_DEBUG)
	MessageBoxW(NULL, lpCmdLine, L"wait for debug", 0);
#endif
	//testLua();
#if !defined(_DEBUG)
	if (lpCmdLine == NULL || lpCmdLine[0] == '\0') {
		//没有任何参数无法运行
		return	0;
	}
#endif
	LOG_FILE(svy::Log::L_INFO,svy::strFormat(_T("run cmd %s"),lpCmdLine));
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
	//读取自己的配置信息
	svy::SinglePtr<AppModule> app;
	AppModule::REG_INFO regInfo;
	PreLogic			prelogic;	
	app->getMySlefModule();
	prelogic.parseCommondLine();
	if (1 >= app->getModuleCount()) {
		return 0;
	}
	if ( !prelogic.canRunUpdate() ) {
		//建立copy
		CString dirAppD = svy::GetLocalAppDataPath();
		CString dirSrc = svy::GetAppPath();
		CString dirDst = svy::catUrl(dirAppD, svy::GetAppName());
		svy::CopyDir(dirSrc, dirDst);
		//
		dirAppD = dirDst; //缓存运行目录
		dirDst = svy::catUrl(dirDst, svy::GetAppName()) + _T(".exe");
		CString strCmd;
		CString strPrevCmds((LPCTSTR)CW2CT(lpCmdLine));
		strPrevCmds.TrimRight();
		if ('|' != strPrevCmds[strPrevCmds.GetLength() - 1]) {
			strCmd.Format(_T("%s | version:%s %s |"), strPrevCmds,
				app->getMySlefModule().mVer_.mVer_, cmdSelfKey() );
		}
		else {
			strCmd.Format(_T("%s version:%s %s |"), strPrevCmds,
				app->getMySlefModule().mVer_.mVer_, cmdSelfKey() );
		}
		regInfo.path = dirSrc;
		regInfo.name = svy::GetAppName();
		//将自己位置信息写入注册表
		AppModule::SaveRegisteInfo(regInfo);		
		::ShellExecute(NULL, _T("open"), dirDst,strCmd,dirAppD,0);
		return 0;
	}
	//初始化lua模块
	lua_State *L = app->getLua();
	//初始化libcurl环境
	svy::CHttpClient::GlobalSetup();
	//判断是否运行copy
	//访问服务器检查更新
	svy::SinglePtr<UpdateSchedule>	updateSchedule;

	updateSchedule->run();

	svy::CHttpClient::GlobalClean();
	return 0;
}