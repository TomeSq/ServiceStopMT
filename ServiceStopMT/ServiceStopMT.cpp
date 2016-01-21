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

	//�T�[�r�X��~�v�����s
	if (!::ControlService(lpParam->serviceHandle,
		SERVICE_CONTROL_STOP,
		(LPSERVICE_STATUS)&ssp)) {
		return 1;
	}

	while (SERVICE_STOPPED != ssp.dwCurrentState)
	{
		//�T�[�r�X��Ԃ̎擾
		if (!::QueryServiceStatusEx(lpParam->serviceHandle,
			SC_STATUS_PROCESS_INFO,
			(LPBYTE)&ssp,
			sizeof(SERVICE_STATUS_PROCESS),
			&dwBytesNeeded))
		{
			return 1;
		}

		//��~�҂��Ȃ班��sleep���čēx�T�[�r�X��ԃ`�F�b�N
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
	//�����`�F�b�N
	if (argc < 2) {
		::wprintf(L"%s servicename, [servicename...]", argv[0]);
		return 1;
	}

	SC_HANDLE scManager;

	//�T�[�r�X�}�l�[�W���[�Ɛڑ�����
	scManager = ::OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (NULL == scManager) {
		::wprintf(L"OpenSCManager failed (%d)\n", ::GetLastError());
		return 1;
	}

	//�����Ŏw�肳�ꂽ�T�[�r�X�̃n���h�����擾����B
	std::vector<SC_HANDLE> serviceHandle(argc - 1);
	int enableServiceCnt = 0;
	for (size_t i = 0; i < serviceHandle.size(); i++) {
		serviceHandle[i] = ::OpenServiceW(scManager, argv[i + 1], SERVICE_STOP | SERVICE_QUERY_STATUS);
		if (NULL == serviceHandle[i]) {
			::wprintf(L"OpenService %s failed (%d)\n", argv[i + 1], GetLastError());
		}
		else {
			enableServiceCnt++; //�L���ȃT�[�r�X�����J�E���g
		}
	}

	if (0 >= enableServiceCnt) {
		::wprintf(L"Not Services\n");
		::CloseServiceHandle(scManager);
		return 0;
	}

	//�X���b�h�̔z���p�ӂ���
	std::vector<HANDLE> hThreads(enableServiceCnt);
	std::vector<unsigned> thredIds(enableServiceCnt);
	std::vector<PARAM> thredParams(enableServiceCnt);


	//�T�[�r�X���X���b�h���쐬���Ĕ񓯊��Œ�~������
	int thCnt = 0;
	for (auto it = serviceHandle.begin(); it != serviceHandle.end(); ++it) {
		if (NULL != *it) {
			thredParams[thCnt].serviceHandle = *it;
			hThreads[thCnt] = (HANDLE)::_beginthreadex(NULL, 0, ServiceStop, &thredParams[thCnt], CREATE_SUSPENDED, &thredIds[thCnt]);
			thCnt++;
		}
	}

	//�X���b�h�J�n
	for (size_t i = 0; i < hThreads.size(); i++) {
		::ResumeThread(hThreads[i]);
	}

	//�X���b�h�I��
	DWORD ret = ::WaitForMultipleObjects(hThreads.size(), hThreads.data(), true, INFINITE);
	if (WAIT_OBJECT_0 == ret) {

	}

	//�X���b�h�n���h������
	for (auto it = hThreads.begin(); it != hThreads.end(); ++it) {
		::CloseHandle(*it);
		*it = NULL;
	}

	//�T�[�r�X�n���h������
	for (auto it = serviceHandle.begin(); it != serviceHandle.end(); ++it) {
		if (NULL == *it) {
			::CloseServiceHandle(*it);
			*it = NULL;
		}
	}

	//SCM�n���h������
	::CloseServiceHandle(scManager);

	return 0;
}

