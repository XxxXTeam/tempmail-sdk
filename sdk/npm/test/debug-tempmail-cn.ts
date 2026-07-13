import WebSocket from "ws";

const HOST = "tempmail.cn";
const VERSIONS = [4, 3];
const TIMEOUT = 15000;

function socketUrl(eio: number): string {
  return `wss://${HOST}/socket.io/?EIO=${eio}&transport=websocket`;
}

async function testConnection(eio: number): Promise<void> {
  console.log(`\n尝试 EIO=${eio}...`);
  const url = socketUrl(eio);
  console.log(`  URL: ${url}`);

  return new Promise<void>((resolve) => {
    const ws = new WebSocket(url, {
      handshakeTimeout: TIMEOUT,
      headers: {
        "User-Agent":
          "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
        Origin: `https://${HOST}`,
        Referer: `https://${HOST}/`,
      },
    });

    const timer = setTimeout(() => {
      console.log("  ⚠️ 总超时");
      ws.close();
      resolve();
    }, TIMEOUT);

    let sentConnect = false;
    let sentRequestShortId = false;

    ws.on("open", () => {
      console.log("  WebSocket 连接成功");
    });

    ws.on("message", (data: WebSocket.RawData) => {
      const packet = data.toString();
      console.log(
        `  收到: ${packet.substring(0, 100)}${packet.length > 100 ? "..." : ""}`,
      );

      // EIO ping
      if (packet === "2") {
        ws.send("3");
        console.log("  发送: 3 (pong)");
        return;
      }

      // EIO open packet
      if (!sentConnect && packet.startsWith("0")) {
        console.log("  ✅ EIO 握手成功");
        ws.send("40");
        console.log("  发送: 40 (Socket.IO connect)");
        sentConnect = true;
        return;
      }

      // Socket.IO connect ack
      if (packet.startsWith("40")) {
        console.log("  ✅ Socket.IO 连接确认");
        if (!sentRequestShortId) {
          sentRequestShortId = true;
          const msg = '42["request shortid",true]';
          ws.send(msg);
          console.log(`  发送: ${msg}`);
        }
        return;
      }

      // Socket.IO event
      if (packet.startsWith("42")) {
        try {
          const decoded = JSON.parse(packet.slice(2));
          console.log(
            `  ✅ 事件: ${decoded[0]}, 数据: ${JSON.stringify(decoded[1]).substring(0, 100)}`,
          );
          if (decoded[0] === "shortid") {
            console.log(`  ✅ 获取到 shortid: ${decoded[1]}`);
            clearTimeout(timer);
            ws.close();
            resolve();
            return;
          }
        } catch {}
      }
    });

    ws.on("error", (err: Error) => {
      console.log(`  ❌ WebSocket 错误: ${err.message}`);
      clearTimeout(timer);
      resolve();
    });

    ws.on("close", (code: number, reason: Buffer) => {
      console.log(`  WebSocket 关闭: code=${code} reason=${reason.toString()}`);
      clearTimeout(timer);
      resolve();
    });
  });
}

async function main() {
  console.log("=== TempMail-CN WebSocket 调试 ===");
  for (const v of VERSIONS) {
    await testConnection(v);
  }
}

main().catch(console.error);
