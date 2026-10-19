#include "DialogAboutCSSE.h"

namespace se::cs {
	IMPLEMENT_DYNAMIC(DialogAboutCSSE, CDialogEx);

	DialogAboutCSSE::DialogAboutCSSE(CWnd* pParent) :
		CDialogEx(DIALOG_CSSE_ABOUT, pParent)
	{

	}

	DialogAboutCSSE::~DialogAboutCSSE() {

	}

	BOOL DialogAboutCSSE::OnInitDialog() {
		CDialogEx::OnInitDialog();

		// Update build number.
#ifdef CSSE_BUILD_NUMBER
		std::stringstream ss;
		ss << "Build " << CSSE_BUILD_NUMBER;
		auto label = GetDlgItem(IDC_BUILD_INFO_STATIC);
		label->SetWindowTextA(ss.str().c_str());
#endif

		return TRUE;
		// return TRUE unless you set the focus to a control
		// EXCEPTION: OCX Property Pages should return FALSE
	}

	void DialogAboutCSSE::DoDataExchange(CDataExchange* pDX) {
		CDialogEx::DoDataExchange(pDX);
	}
}

BEGIN_MESSAGE_MAP(se::cs::DialogAboutCSSE, CDialogEx)
END_MESSAGE_MAP()

