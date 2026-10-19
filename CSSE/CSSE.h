#pragma once

namespace se::cs {
	class CSSE : public CWinApp {
	public:
		CSSE();

	public:
		virtual BOOL InitInstance();
		virtual int ExitInstance();

		DECLARE_MESSAGE_MAP()

	private:
		void InstallPatches() const;
		void UpdateCurrentDirectory() const;
	};

	extern CSSE application;
}