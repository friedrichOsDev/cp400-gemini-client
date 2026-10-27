#include <appdef.hpp>
#include <sdk/calc/calc.hpp>
#include <sdk/os/debug.hpp>
#include <sdk/os/lcd.hpp>
#include <sdk/os/serial.hpp>
#include <sdk/os/gui.hpp>
#include <stdint.h>
#include <stddef.h>

APP_NAME("Gemini Chat")
APP_DESCRIPTION("Gemini Chat Client for the CASIO FX-CP400")
APP_AUTHOR("friedrichOsDev")
APP_VERSION("1.0")

enum {
    ID_BTN_SEND = 101,
    ID_BTN_SHOW_ANSWER = 102
};

static char g_geminiResponse[1024] = "Noch keine Antwort empfangen.";
static bool g_hasGeminiResponse = false;
static int g_statusIndex = 0;
static const char* const g_waitingPhrases[15] = {
    "Loading brain...",
    "Feeding the AI...",
    "Warming up chips...",
    "Doing math magic...",
    "Counting pixels...",
    "Consulting oracle...",
    "Charging lasers...",
    "Searching reality...",
    "Polishing answers...",
    "Baking code...",
    "Connecting dots...",
    "Solving universe...",
    "Waking up bots...",
    "Mining ideas...",
    "Just a second..."
};

static void CopyText(char* destination, size_t capacity, const char* source) {
    if (destination == nullptr || capacity == 0) {
        return;
    }

    size_t i = 0;

    if (source != nullptr) {
        while (source[i] != '\0' && i + 1 < capacity) {
            destination[i] = source[i];
            ++i;
        }
    }

    destination[i] = '\0';
}

class GeminiAnswerDialog : public GUIDialog {
public:
    GUILongLabel lblAnswer;

    GeminiAnswerDialog()
        : GUIDialog(
            Height95,
            AlignCenter,
            "Gemini Antwort",
            KeyboardStateNone
          ),
          lblAnswer(
              GetLeftX() + 15,
              GetTopY() + 15,
              GetRightX() - 15,
              GetBottomY() - 15,
              g_geminiResponse
          )
    {
        AddElement(lblAnswer);
    }
};

class GeminiChatDialog : public GUIDialog {
public:
    GUILabel lblTitle;
    GUILabel lblPromptHint;
    GUITextBox txtInput;
    GUIButton btnSend;
    GUIButton btnShowAnswer;
    GUILongLabel lblStatus;

    GeminiChatDialog()
        : GUIDialog(
            Height95,
            AlignCenter,
            "Gemini Chat",
            KeyboardStateABC
          ),
          lblTitle(
              GetLeftX() + 15,
              GetTopY() + 15,
              "Ask Gemini:"
          ),
          lblPromptHint(
              GetLeftX() + 15,
              GetTopY() + 35,
              "Text hier eingeben:"
          ),
          txtInput(
              GetLeftX() + 15,
              GetTopY() + 55,
              GetRightX() - GetLeftX() - 30,
              "",
              64,
              true
          ),
          btnSend(
              GetLeftX() + 15,
              GetTopY() + 100,
              GetRightX() - 15,
              GetTopY() + 130,
              "Senden",
              ID_BTN_SEND,
              GUIButton::FlagEnabled
          ),
          btnShowAnswer(
              GetLeftX() + 15,
              GetTopY() + 140,
              GetRightX() - 15,
              GetTopY() + 170,
              "Show Answer",
              ID_BTN_SHOW_ANSWER,
              GUIButton::FlagEnabled
          ),
          lblStatus(
              GetLeftX() + 15,
              GetTopY() + 180,
              GetRightX() - 15,
              GetBottomY() - 15,
              "Bereit."
          )
    {
        AddElement(lblTitle);
        AddElement(lblPromptHint);
        AddElement(txtInput);
        AddElement(btnSend);
        AddElement(btnShowAnswer);
        AddElement(lblStatus);
    }

    void CommunicateWithESP(const char* prompt) {
        if (prompt == nullptr || prompt[0] == '\0') {
            lblStatus.SetText("Bitte eine Frage eingeben.");
            Refresh();
            return;
        }

        lblStatus.SetText(g_waitingPhrases[g_statusIndex]);
        g_statusIndex = (g_statusIndex + 1) % 15;
        Refresh();

        Serial_ClearRX();

        int len = 0;
        while (prompt[len] != '\0') {
            ++len;
        }

        Serial_Write(
            reinterpret_cast<const unsigned char*>(prompt),
            len
        );

        const unsigned char newline = '\n';
        Serial_Write(&newline, 1);

        char rx_buf[sizeof(g_geminiResponse)];
        int rx_len = 0;
        rx_buf[0] = '\0';

        bool lineEnded = false;

        for (volatile uint32_t timeout = 0; timeout < 200000000UL; ++timeout) {
            int available = Serial_PollRX();

            if (available > 0) {
                unsigned char b;

                if (Serial_ReadSingle(&b) == 0) {
                    if (b == '\n' || b == '\r') {
                        if (rx_len > 0) {
                            lineEnded = true;
                            break;
                        }
                    } else if (rx_len < static_cast<int>(sizeof(rx_buf)) - 1) {
                        rx_buf[rx_len++] = static_cast<char>(b);
                        rx_buf[rx_len] = '\0';
                    } else {
                        break;
                    }
                }
            }
        }

        if (rx_len > 0) {
            CopyText(g_geminiResponse, sizeof(g_geminiResponse), rx_buf);
            g_hasGeminiResponse = true;

            if (lineEnded) {
                lblStatus.SetText("Antwort empfangen. Tippe auf Show Answer.");
            } else {
                lblStatus.SetText("Antwort teilweise empfangen. Show Answer oeffnen.");
            }
        } else {
            CopyText(
                g_geminiResponse,
                sizeof(g_geminiResponse),
                "Keine Antwort empfangen. Bitte Verbindung pruefen."
            );
            g_hasGeminiResponse = false;
            lblStatus.SetText("Timeout: Keine Antwort empfangen.");
        }

        Refresh();
    }

    virtual int OnEvent(
        struct GUIDialog_Wrapped* dialog,
        struct GUIDialog_OnEvent_Data* event
    ) override {
        const uint16_t eventID = event->GetEventID();

        if (eventID == ID_BTN_SEND) {
            char prompt[128];
            CopyText(prompt, sizeof(prompt), txtInput.GetText());

            CommunicateWithESP(prompt);
            return 0;
        }

        if (eventID == ID_BTN_SHOW_ANSWER) {
            if (!g_hasGeminiResponse) {
                GUI_DisplayMessageBox_Internal(
                    0,
                    "Gemini",
                    "Noch keine Antwort",
                    "Sende zuerst eine Frage an Gemini.",
                    BUTTON_OK,
                    false
                );
                return 0;
            }

            GeminiAnswerDialog answerDialog;
            answerDialog.ShowDialog();

            Refresh();
            return 0;
        }

        return GUIDialog::OnEvent(dialog, event);
    }
};

extern "C" void main() {
    calcInit();

    // Hollyhock-2 Serial mode: 115200 baud.
    unsigned char mode[6] = {0, 9, 0, 0, 0, 0};

    if (Serial_IsOpen()) {
        Serial_Close(1);
    }

    if (Serial_Open(mode) != 0) {
        GUI_DisplayMessageBox_Internal(
            0,
            "Fehler",
            "Serielle Schnittstelle",
            "Oeffnen fehlgeschlagen!",
            BUTTON_OK,
            false
        );
        calcEnd();
        return;
    }

    {
        GeminiChatDialog chatDialog;
        chatDialog.ShowDialog();
    }

    Serial_Close(1);
    calcEnd();
}
