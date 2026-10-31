; Tabula Rasa mouselook fix: test moves for Windows (AutoHotkey v2, https://www.autohotkey.com).
;
; Start this script, start the game, and stand somewhere with a clear landmark in front of you.
; In mouselook (crosshair mode), press:
;
;   F6   slow:  240 moves of 1 pixel to the right (25 ms apart), then wait
;   F7   jump:  1 move of 240 pixels to the left (undoes F6 if nothing was lost)
;   F8   slow:  120 moves of 1 pixel down (25 ms apart), then wait
;   F9   jump:  1 move of 120 pixels up (undoes F8 if nothing was lost)
;   F10  slow:  120 moves of 2 pixels to the right (240 in total), then wait
;   F11  slow:  60 moves of 4 pixels to the right (240 in total), then wait
;   F12  exit this script
;
; The moves are relative mouse moves, like a real mouse. Turn OFF "Enhance pointer precision"
; (Windows mouse settings, Additional mouse options, Pointer Options) while you test, and keep the
; pointer speed slider in the middle (6th of 11 marks): then one move of 1 is exactly 1 pixel.
; Turn it back on afterwards if you use it.

#Requires AutoHotkey v2.0
#SingleInstance Force

Move(dx, dy) {
    ; mouse_event(MOUSEEVENTF_MOVE, dx, dy): a relative move, like a real mouse
    DllCall("mouse_event", "UInt", 0x0001, "Int", dx, "Int", dy, "UInt", 0, "UPtr", 0)
}

Steps(count, dx, dy) {
    Loop count {
        Move(dx, dy)
        Sleep 25
    }
    SoundBeep 880, 80
}

F6:: Steps(240, 1, 0)
F7:: (Move(-240, 0), SoundBeep(660, 80))
F8:: Steps(120, 0, 1)
F9:: (Move(0, -120), SoundBeep(660, 80))
F10:: Steps(120, 2, 0)
F11:: Steps(60, 4, 0)
F12:: ExitApp
