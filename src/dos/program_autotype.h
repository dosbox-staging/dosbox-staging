#include "programs.h"

class AUTOTYPE : public Program {
	public:
		void Run();
	private:
		void PrintUsage();
		void PrintKeys();
		bool ReadDoubleArg(const std::string &name, 
                           const char        *flag,
                           const double      &def_value,
                           const double      &min_value,
                           const double      &max_value,
                           double            &value);
};

void AUTOTYPE_ProgramStart(Program * *make);