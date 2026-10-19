#pragma once

namespace se::cs {
	class DialogAboutCSSE : public CDialogEx {
		DECLARE_DYNAMIC(DialogAboutCSSE)

	public:
		DialogAboutCSSE(CWnd* pParent = nullptr);   // standard constructor
		virtual ~DialogAboutCSSE();

		// Dialog Data
#ifdef AFX_DESIGN_TIME
		enum { IDD = DIALOG_CSSE_ABOUT };
#endif

		virtual BOOL OnInitDialog();

	protected:
		virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

		DECLARE_MESSAGE_MAP()
	};
}
