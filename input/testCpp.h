#import <vector>
#import <string>

using namespace std;

class DynamicIDHandler {
public:
    DynamicIDHandler(const char *Sema)
    : m_Sema(Sema) {}
    ~DynamicIDHandler();
    
    /// \brief Provides last resort lookup for failed unqualified lookups
    ///
    /// If there is failed lookup, tell sema to create an artificial declaration
    /// which is of dependent type. So the lookup result is marked as dependent
    /// and the diagnostics are suppressed. After that is's an interpreter's
    /// responsibility to fix all these fake declarations and lookups.
    /// It is done by the DynamicExprTransformer.
    ///
    /// @param[out] R The recovered symbol.
    /// @param[in] S The scope in which the lookup failed.
    bool LookupUnqualified(int &R, int *S);
public:
    std::string m_Sema;
};

void function(int a, int b);

