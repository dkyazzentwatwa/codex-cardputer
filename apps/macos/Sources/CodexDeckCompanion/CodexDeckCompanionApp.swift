import AppKit
import SwiftUI

@main
struct CodexDeckCompanionApp: App {
    @StateObject private var store = CompanionStore()

    var body: some Scene {
        MenuBarExtra {
            CompanionMenuView(store: store)
        } label: {
            HStack(spacing: 4) {
                DeckMark(status: store.lifecycle.color)
                    .frame(width: 18, height: 18)
                Text("Deck")
                    .font(.system(size: 11, weight: .semibold, design: .rounded))
            }
                .accessibilityLabel("CodexDeck \(store.lifecycle.label)")
        }
        .menuBarExtraStyle(.window)

        Settings {
            SettingsRootView(store: store)
        }
    }
}

func openCompanionSettings() {
    NSApp.sendAction(Selector(("showSettingsWindow:")), to: nil, from: nil)
    NSApp.activate(ignoringOtherApps: true)
}
