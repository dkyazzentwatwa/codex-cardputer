import SwiftUI

/// The compact Cardputer-inspired identity used in the menu bar and app UI.
struct DeckMark: View {
    var status: Color

    var body: some View {
        GeometryReader { proxy in
            let width = proxy.size.width
            let height = proxy.size.height
            let corner = max(3, width * 0.16)

            ZStack(alignment: .bottomTrailing) {
                RoundedRectangle(cornerRadius: corner, style: .continuous)
                    .fill(Color(red: 0.035, green: 0.06, blue: 0.08))
                    .overlay {
                        RoundedRectangle(cornerRadius: corner, style: .continuous)
                            .stroke(Color.white.opacity(0.22), lineWidth: max(0.8, width * 0.055))
                    }

                VStack(spacing: height * 0.08) {
                    RoundedRectangle(cornerRadius: max(1, width * 0.05), style: .continuous)
                        .fill(Color.cyan.opacity(0.9))
                        .overlay {
                            HStack(spacing: width * 0.035) {
                                Capsule().fill(Color.black.opacity(0.6))
                                Capsule().fill(Color.black.opacity(0.6))
                                Capsule().fill(Color.black.opacity(0.6))
                            }
                            .padding(.horizontal, width * 0.12)
                            .padding(.vertical, height * 0.065)
                        }
                        .frame(height: height * 0.38)

                    HStack(spacing: width * 0.055) {
                        ForEach(0..<4, id: \.self) { _ in
                            RoundedRectangle(cornerRadius: max(0.7, width * 0.025), style: .continuous)
                                .fill(Color.white.opacity(0.75))
                        }
                    }
                    .frame(height: height * 0.15)
                }
                .padding(width * 0.16)

                Circle()
                    .fill(status)
                    .overlay { Circle().stroke(Color.black.opacity(0.75), lineWidth: max(0.8, width * 0.05)) }
                    .frame(width: width * 0.34, height: width * 0.34)
                    .offset(x: width * 0.08, y: height * 0.08)
            }
        }
        .aspectRatio(1, contentMode: .fit)
        .accessibilityHidden(true)
    }
}
