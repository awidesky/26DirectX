#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string>

enum class LogLevel
{
    Info,
    Warning,
    Error
};

class Logger
{
private:
    FILE* file;
    static Logger* instance;

    // 싱글톤으로 설계하여 어디서든 접근 가능하게 함
    Logger() : file(nullptr)
    {
    }

public:
    static Logger* Get()
    {
        if (instance == nullptr)
        {
            instance = new Logger();
        }
        return instance;
    }

    // 로그 파일 열기
    bool Initialize(const std::string& filename)
    {
        // 보수적으로 이전 파일이 있으면 덮어쓰기(w) 모드
        file = fopen(filename.c_str(), "w");
        return file != nullptr;
    }

    void Log(LogLevel level, const char* format, ...)
    {
        if (file == nullptr) return;

        // 1. 시간 계산
        time_t now = time(0);
        struct tm tstruct;
        char timeBuf[80];
        localtime_s(&tstruct, &now);
        strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &tstruct);

        // 2. 레벨 문자열 변환
        const char* levelStr = "";
        switch (level)
        {
        case LogLevel::Info:    levelStr = "[INFO]"; break;
        case LogLevel::Warning: levelStr = "[WARN]"; break;
        case LogLevel::Error:   levelStr = "[ERR ]"; break;
        }

        // 3. 가변 인자 처리 (printf 스타일)
        char messageBuf[1024];
        va_list args;
        va_start(args, format);
        vsnprintf(messageBuf, sizeof(messageBuf), format, args);
        va_end(args);

        // 4. 파일 출력 및 콘솔 동시 출력
        fprintf(file, "%s %s %s\n", timeBuf, levelStr, messageBuf);
        printf("%s %s %s\n", timeBuf, levelStr, messageBuf);

        // 즉시 파일에 쓰도록 밀어냄 (Crash 대비)
        fflush(file);
    }

    ~Logger()
    {
        if (file)
        {
            fclose(file);
        }
    }
};

// static 인스턴스 초기화
Logger* Logger::instance = nullptr;


int main()
{
    Logger::Get()->Initialize("EngineLog.txt");
    Logger::Get()->Log(LogLevel::Info, "Engine Initializing...");

    // 쉐이더가 에러났다 치고...
    Logger::Get()->Log(LogLevel::Error, "Shader Compile Failed! Check your .hlsl path.");
    

    return 0;
}