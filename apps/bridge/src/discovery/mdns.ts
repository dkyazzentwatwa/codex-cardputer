import Bonjour from "bonjour-service";

import { PROTOCOL_VERSION } from "@codexdeck/protocol";

export class MdnsAdvertiser {
  private readonly bonjour = new Bonjour();
  private service: ReturnType<Bonjour["publish"]> | undefined;

  start(port: number, host: string, bridgeVersion: string): void {
    this.service = this.bonjour.publish({
      name: "CardPuter Codex Control Deck",
      type: "codexdeck",
      protocol: "tcp",
      port,
      host,
      disableIPv6: true,
      txt: { protocol: PROTOCOL_VERSION, version: bridgeVersion },
    });
  }

  stop(): void {
    this.service?.stop();
    this.service = undefined;
    this.bonjour.destroy();
  }
}
