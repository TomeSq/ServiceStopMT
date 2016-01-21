#include "stdafx.h"

#include <Windows.h>
#include <stdio.h>
#include <process.h>
#include <vector>

#define BUF_SIZ 256

typedef struct {
	SC_HANDLE serviceHandle;
} PARAM;

unsigned __stdcall ServiceStop(void* params)
{
	PARAM* lpParam = (PARAM*)params;
	SERVICE_STATUS_PROCESS ssp;
	DWORD dwBytesNeeded;
	DWORD dwWaitTime;

	//サービス停止要求発行
	if (!::ControlService(lpParam->serviceHandle,
		SERVICE_CONTROL_STOP,
		(LPSERVICE_STATUS)&ssp)) {
		return 1;
	}

	while (SERVICE_STOPPED != ssp.dwCurrentState)
	{
		//サービス状態の取得
		if (!::QueryServiceStatusEx(lpParam->serviceHandle,
			SC_STATUS_PROCESS_INFO,
			(LPBYTE)&ssp,
			sizeof(SERVICE_STATUS_PROCESS),
			&dwBytesNeeded))
		{
			return 1;
		}

		//停止待ちなら少しsleepして再度サービス状態チェック
		switch (ssp.dwCurrentState)
		{
		case SERVICE_STOPPED:
			break;
		case SERVICE_STOP_PENDING:
			dwWaitTime = ssp.dwWaitHint;
			if (dwWaitTime > 1000) {
				dwWaitTime = 100;
			}
			::Sleep(dwWaitTime);
			break;
		default:
			break;
		}

	}

	return 0;
}


int wmain(int argc, wchar_t *argv[])
{
	//引数チェック
	if (argc < 2) {
		::wprintf(L"%s servicename, [servicename...]", argv[0]);
		return 1;
	}

	SC_HANDLE scManager;

	//サービスマネージャーと接続する
	scManager = ::OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (NULL == scManager) {
		::wprintf(L"OpenSCManager failed (%d)\n", ::GetLastError());
		return 1;
	}

	//引数で指定されたサービスのハンドルを取得する。
	std::vector<SC_HANDLE> serviceHandle(argc - 1);
	int enableServiceCnt = 0;
	for (size_t i = 0; i < serviceHandle.size(); i++) {
		serviceHandle[i] = ::OpenServiceW(scManager, argv[i + 1], SERVICE_STOP | SERVICE_QUERY_STATUS);
		if (NULL == serviceHandle[i]) {
			::wprintf(L"OpenService %s failed (%d)\n", argv[i + 1], GetLastError());
		}
		else {
			enableServiceCnt++; //有効なサービス数をカウント
		}
	}

	if (0 >= enableServiceCnt) {
		::wprintf(L"Not Services\n");
		::CloseServiceHandle(scManager);
		return 0;
	}

	//スレッドの配列を用意する
	std::vector<HANDLE> hThreads(enableServiceCnt);
	std::vector<unsigned> thredIds(enableServiceCnt);
	std::vector<PARAM> thredParams(enableServiceCnt);


	//サービス分スレッドを作成して非同期で停止させる
	int thCnt = 0;
	for (auto it = serviceHandle.begin(); it != serviceHandle.end(); ++it) {
		if (NULL != *it) {
			thredParams[thCnt].serviceHandle = *it;
			hThreads[thCnt] = (HANDLE)::_beginthreadex(NULL, 0, ServiceStop, &thredParams[thCnt], CREATE_SUSPENDED, &thredIds[thCnt]);
			thCnt++;
		}
	}

	//スレッド開始
	for (size_t i = 0; i < hThreads.size(); i++) {
		::ResumeThread(hThreads[i]);
	}

	//スレッド終了
	DWORD ret = ::WaitForMultipleObjects(hThreads.size(), hThreads.data(), true, INFINITE);
	if (WAIT_OBJECT_0 == ret) {

	}

	//スレッドハンドル閉じる
	for (auto it = hThreads.begin(); it != hThreads.end(); ++it) {
		::CloseHandle(*it);
		*it = NULL;
	}

	//サービスハンドル閉じる
	for (auto it = serviceHandle.begin(); it != serviceHandle.end(); ++it) {
		if (NULL == *it) {
			::CloseServiceHandle(*it);
			*it = NULL;
		}
	}

	//SCMハンドル閉じる
	::CloseServiceHandle(scManager);

	return 0;
}

