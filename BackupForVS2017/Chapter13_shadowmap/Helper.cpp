#include<string>
#include<assert.h>
#include "Helper.h"


using namespace std;

Helper::Helper()
{
}


Helper::~Helper()
{
}

//�P�o�C�gstring�����C�h����wstring�ɕϊ�����
wstring WStringFromString(const std::string& str) {
	wstring wstr;
	auto wcharNum = MultiByteToWideChar(CP_ACP, 0, str.c_str(), str.length(), nullptr, 0);
	wstr.resize(wcharNum);
	wcharNum = MultiByteToWideChar(CP_ACP, 0, str.c_str(), str.length(),
		&wstr[0], wstr.size());
	return wstr;
}

///�g���q��Ԃ�
///@param path ���̃p�X������
///@return �g���q������
wstring GetExtension(const wstring& path) {
	int index = path.find_last_of(L'.');
	return path.substr(index + 1, path.length() - index);
}

bool CheckResult(HRESULT &result, ID3DBlob * errBlob)
{
	if (FAILED(result)) {
#ifdef _DEBUG
		if (errBlob!=nullptr) {
			std::string outmsg;
			outmsg.resize(errBlob->GetBufferSize());
			std::copy_n(static_cast<char*>(errBlob->GetBufferPointer()),
				errBlob->GetBufferSize(),
				outmsg.begin());
			OutputDebugString(outmsg.c_str());//�o�̓E�B���h�E�ɏo�͂��Ă�
		}
		assert(SUCCEEDED(result));
#endif
		return false;
	}
	else {
		return true;
	}
}

unsigned int
AligmentedValue(unsigned int size, unsigned int alignment) {
	return (size + alignment - (size%alignment));
}

std::vector<float> 
GetGaussianValues(float s, size_t sampleNum) {
	std::vector<float> weight(sampleNum);
	float total = 0;//�ォ�犄�邽�߂ɍ��v�l���L�^
	for (int i = 0; i < sampleNum; ++i) {
		float x = static_cast<float>(i);
		auto wgt= expf(-(x * x) / (2 * s*s));
		weight[i] = wgt;
		total += wgt;
	}
	//�����܂ł��ƁA�E���������Ȃ̂�
	//������(���E�Ώ̂Ȃ̂ŁA�f�[�^�͂���Ȃ�)
	//�ł��g�[�^���͍Čv�Z�B�Q�{���ăf�[�^0�Ԃ�
	//�d�����Ă��邽�߁A���������
	total = total * 2 - weight[0];
	for (auto& wgt : weight) {
		wgt /= total;
	}

	return weight;

}