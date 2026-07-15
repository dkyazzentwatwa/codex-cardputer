export type RpcId = number | string;

export interface RpcErrorShape {
  code: number;
  message: string;
  data?: unknown;
}

export interface AppServerThread {
  id: string;
  name?: string | null;
}

export interface AppServerTurn {
  id: string;
  status?: string;
}

export type AppServerUserInput =
  | { type: "text"; text: string; text_elements: [] }
  | { type: "skill"; name: string; path: string };

export interface AppServerServerRequest {
  id: RpcId;
  method: string;
  params: unknown;
  respond(result: unknown): void;
  reject(code: number, message: string): void;
}

export interface AppServerNotification {
  method: string;
  params: unknown;
}

export interface SkillMetadata {
  name: string;
  path: string;
  enabled: boolean;
  description?: string;
}
