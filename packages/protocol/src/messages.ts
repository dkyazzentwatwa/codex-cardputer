import type { z } from "zod";

import type {
  approvalDecisionSchema,
  approvalRequestSchema,
  deviceMessageSchema,
  macroDescriptorSchema,
  serverMessageSchema,
  taskStatusSchema,
  taskSummarySchema,
} from "./schemas.js";

export type TaskStatus = z.infer<typeof taskStatusSchema>;
export type MacroDescriptor = z.infer<typeof macroDescriptorSchema>;
export type TaskSummary = z.infer<typeof taskSummarySchema>;
export type ApprovalDecision = z.infer<typeof approvalDecisionSchema>;
export type ApprovalRequest = z.infer<typeof approvalRequestSchema>;
export type DeviceMessage = z.infer<typeof deviceMessageSchema>;
export type ServerMessage = z.infer<typeof serverMessageSchema>;
export type MutatingDeviceMessage = Extract<DeviceMessage, { requestId: string }>;
