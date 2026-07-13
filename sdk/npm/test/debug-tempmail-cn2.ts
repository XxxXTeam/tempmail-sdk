import WebSocket from "ws";

const HOST = "tempmail.cn";

async function testNewEvents(): Promise<void> {
  console.log("测试 tempmail-cn 新事件名...");
  const url = `wss://${HOST}/socket.io/?EIO=4&transport=websocket`;

  return new Promise<void>((resolve) => {
    const ws = new WebSocket(url, {
      handshakeTimeout: 15000,
      headers: {
        "User-Agent":
          "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
        Origin: `https://${HOST}`,
        Referer: `https://${HOST}/`,
      },
    });

    const timer = setTimeout(() => {
      console.log("⚠️ 超时");
      ws.close();
      resolve();
    }, 20000);

    let sentConnect = false;
    let sentRequest = false;

    ws.on("message", (data: WebSocket.RawData) => {
      const packet = data.toString();
      console.log(`收到: ${packet.substring(0, 200)}`);

      if (packet === "2") {
        ws.send("3");
        return;
      }

      if (!sentConnect && packet.startsWith("0")) {
        ws.send("40");
        sentConnect = true;
        return;
      }

      if (packet.startsWith("40") && !sentRequest) {
        sentRequest = true;
        // 使用新的事件名
        const msg = '42["request mailbox"]';
        ws.send(msg);
        console.log(`发送: ${msg}`);
        return;
      }

      if (packet.startsWith("42")) {
        try {
          const decoded = JSON.parse(packet.slice(2));
          console.log(`✅ 事件: ${decoded[0]}`);
          console.log(`   数据: ${JSON.stringify(decoded[1])}`);
          clearTimeout(timer);
          ws.close();
          resolve();
        } catch {}
      }
    });

    ws.on("error", (e: Error) => {
      console.log(`错误: ${e.message}`);
      clearTimeout(timer);
      resolve();
    });
    ws.on("close", () => {
      clearTimeout(timer);
      resolve();
    });
  });
}

testNewEvents().catch(console.error);
