#pragma once
#include<Windows.h>
#include<memory>

class Dx12Wrapper;

struct Size {
	int width;
	int height;
	Size() {}
	Size(int w, int h) :width(w), height(h) {}
};

class PMDActor;
class PMDRenderer;

class Application
{
private:
	HWND _hwnd;//�܂����̃E�B���h�E�𑀍삷�邽�߂̃n���h������肽��
	WNDCLASSEX _wndClass = {};

	std::shared_ptr<Dx12Wrapper> _dx12;
	std::shared_ptr<PMDActor> _actor;
	std::shared_ptr<PMDRenderer> _pmdRenderer;
	//�R���X�g���N�^��private�ɂ���new�����Ȃ��悤��
	Application();
	//�R�s�[�A����֎~
	Application(const Application&) = delete;
	void operator=(const Application&) = delete;
public:
	static Application& Instance();
	///�A�v���P�[�V����������
	bool Initialize();
	void SyncronizeEffekseerCamera();
	///�A�v���P�[�V�����N��
	void Run();
	///�A�v���P�[�V�����I��
	void Terminate();

	Size GetWindowSize();

	~Application();
};

