#pragma once

//namespace se::cs {
	class DialogCSSESettings : public CDialogEx {
		DECLARE_DYNAMIC(DialogCSSESettings)

	public:
		DialogCSSESettings(CWnd* pParent = nullptr);   // standard constructor
		virtual ~DialogCSSESettings();

		virtual BOOL OnInitDialog();

		afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);

		// Dialog Data
#ifdef AFX_DESIGN_TIME
		enum { IDD = IDD_CSSE_SETTINGS };
#endif

	protected:
		virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

		DECLARE_MESSAGE_MAP()
	public:
		CMFCPropertyGridCtrl m_PropertyGrid;

		CMFCPropertyGridProperty* m_PropertyEnabled = nullptr;
	};
//}
