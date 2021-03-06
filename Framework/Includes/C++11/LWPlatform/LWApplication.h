#ifndef LWAPPLICATION_H
#define LWAPPLICATION_H
#include "LWTypes.h"
#include <functional>

const uint32_t LWSignal_CtrlC = 0; /*!< \brief console has recievied a ctrl+c close event. */
const uint32_t LWSignal_Break = 1; /*!< \brief console has receivied a ctrl+break close event. */
const uint32_t LWSignal_Close = 2; /*!< \brief console has been asked to close nicely(i.e: os x has been clicked. */
const uint32_t LWSignal_Logoff = 3; /*!< \brief user has logged off. */
const uint32_t LWSignal_Shutdown = 4; /*!< \brief platform is shutting off. */
const uint32_t LWSignal_Unknown = 5; /*!< \brief if this value is received it means the signal received was an unknown type. */

/*!< \brief Signal handler function defintion. */
typedef std::function<bool(uint32_t, void*)> LWSignalHandlerFunc;

/*!< \brief this is the new entry point if including LWPlatform library. */
int32_t LWMain(int32_t argc, char **argv);

/*!< \brief registers for different signal capturing and handling.
	 \param SignalHandler callback to be called when signal is caught, pass true if the application is handling the signal, false if not to pass to the next handler.
	 \param Signal the signal the function is to be registered for.
	 \param UserData to be passed to the SingalHandler function.
*/
void LWRegisterSignal(LWSignalHandlerFunc Handler, uint32_t Signal, void *UserData);

/*!< \brief registers a class's method for signal capture and handling. */
template<class Method, class Obj>
void LWRegisterSignalm(Obj *A, Method CB, uint32_t Signal, void *UserData) {
	 LWRegisterSignal(std::bind(CB, A, std::placeholders::_1, std::placeholders::_2), Signal, UserData);
	 return;
}

/*!< \brief attempts to execute the specefied platform, may not be implementing on some platform. */
bool LWExecute(const char *BinaryPath, const char *Parameters);

/*!< \brief in order to support emscripter for web javascript integration, some changes need to be made to infinite running loops, this function runs the specified function in a loop at the specefied interval. The looped function should return false when it wants to quit out of the loop.*/
bool LWRunLoop(std::function<bool(void*)> LoopFunc, uint64_t Frequency, void* UserData);

/*!< \brief attempts to spawn a process to send a email to the targeted email.  may not be implemented on some platforms. 
	 for a more coheisive email system, use Protocol_SMTP or Protocol_POP3(if they have been implemented yet.)
*/
bool LWEmail(const char *SrcEmail, const char *TargetEmail, const char *Subject, const char *Body, const char *SMTPServer, const char *SMTPUsername, const char *SMTPPassword);

/*!< \brief opens a browser window on the platform(if possible), and attempts to set it to the targeted url. */
bool LWBrowser(const char *URL);

/*!< \brief returns the systems current dpi scale. */
float LWSystemScale(void);

#endif