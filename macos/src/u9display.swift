// u9display：列出螢幕、暫時把指定螢幕設為主要顯示器、還原原本排列
//   u9display list              每行：編號<TAB>名稱<TAB>寬x高<TAB>main/空白
//   u9display set-main <編號>   把該螢幕設為主要顯示器，原本排列存到 <狀態檔>
//   u9display restore           依狀態檔還原排列並刪除狀態檔
import AppKit
import CoreGraphics

let stateFile = URL(fileURLWithPath: CommandLine.arguments[0])
    .deletingLastPathComponent().appendingPathComponent(".display-restore")

func fail(_ msg: String) -> Never {
    FileHandle.standardError.write((msg + "\n").data(using: .utf8)!)
    exit(1)
}

func onlineDisplays() -> [CGDirectDisplayID] {
    var count: UInt32 = 0
    CGGetOnlineDisplayList(0, nil, &count)
    var ids = [CGDirectDisplayID](repeating: 0, count: Int(count))
    CGGetOnlineDisplayList(count, &ids, &count)
    // 鏡像中的副螢幕不算
    return ids.filter { CGDisplayMirrorsDisplay($0) == kCGNullDirectDisplay }
}

func name(of id: CGDirectDisplayID) -> String {
    for s in NSScreen.screens {
        if let n = s.deviceDescription[NSDeviceDescriptionKey("NSScreenNumber")] as? NSNumber,
           n.uint32Value == id { return s.localizedName }
    }
    return "Display \(id)"
}

func apply(_ origins: [CGDirectDisplayID: CGPoint]) {
    var cfg: CGDisplayConfigRef?
    guard CGBeginDisplayConfiguration(&cfg) == .success else { fail("無法開始設定螢幕") }
    for (id, p) in origins {
        CGConfigureDisplayOrigin(cfg, id, Int32(p.x), Int32(p.y))
    }
    guard CGCompleteDisplayConfiguration(cfg, .forSession) == .success else { fail("無法套用螢幕設定") }
}

let args = CommandLine.arguments
let displays = onlineDisplays()

switch args.count > 1 ? args[1] : "" {
case "list":
    for (i, id) in displays.enumerated() {
        let b = CGDisplayBounds(id)
        print("\(i + 1)\t\(name(of: id))\t\(Int(b.width))x\(Int(b.height))\t\(CGDisplayIsMain(id) != 0 ? "main" : "")")
    }

case "set-main":
    guard args.count > 2, let n = Int(args[2]), n >= 1, n <= displays.count else { fail("編號錯誤") }
    let target = displays[n - 1]
    if CGDisplayIsMain(target) != 0 { exit(0) }  // 已是主要顯示器，不必動
    var saved: [String: [Double]] = [:]
    for id in displays {
        let o = CGDisplayBounds(id).origin
        saved[String(id)] = [Double(o.x), Double(o.y)]
    }
    try! JSONSerialization.data(withJSONObject: saved).write(to: stateFile)
    // 主要顯示器＝位於 (0,0) 的螢幕；整體平移讓目標落在原點，相對位置不變
    let t = CGDisplayBounds(target).origin
    var origins: [CGDirectDisplayID: CGPoint] = [:]
    for id in displays {
        let o = CGDisplayBounds(id).origin
        origins[id] = CGPoint(x: o.x - t.x, y: o.y - t.y)
    }
    apply(origins)

case "restore":
    guard let data = try? Data(contentsOf: stateFile),
          let saved = try? JSONSerialization.jsonObject(with: data) as? [String: [Double]] else { exit(0) }
    var origins: [CGDirectDisplayID: CGPoint] = [:]
    for id in displays {
        if let p = saved[String(id)] { origins[id] = CGPoint(x: p[0], y: p[1]) }
    }
    apply(origins)
    try? FileManager.default.removeItem(at: stateFile)

default:
    fail("用法：u9display list | set-main <編號> | restore")
}
