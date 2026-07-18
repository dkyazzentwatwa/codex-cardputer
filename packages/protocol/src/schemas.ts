import { z } from "zod";

import {
  MAX_FOLLOWUP_BYTES,
  MAX_SNAPSHOT_TASKS,
  PROTOCOL_VERSION,
} from "./constants.js";

const idSchema = z.string().trim().min(1).max(128);
const isoTimestampSchema = z.iso.datetime({ offset: true });

export const taskStatusSchema = z.enum([
  "starting",
  "running",
  "waiting_approval",
  "waiting_input",
  "completed",
  "failed",
  "cancelled",
  "stale",
]);

export const macroActionSchema = z.enum([
  "launch_workflow",
  "launch_skill",
  "stop_task",
  "send_followup",
  "approve",
  "reject",
  "retry",
  "review_changes",
  "run_tests",
  "commit_changes",
]);

export const confirmationSchema = z.enum(["none", "press_again", "hold"]);

export const macroDescriptorSchema = z
  .object({
    id: idSchema,
    label: z.string().trim().min(1).max(48),
    shortLabel: z.string().trim().min(1).max(16),
    keyHint: z.string().trim().min(1).max(8).optional(),
    action: macroActionSchema,
    confirmation: confirmationSchema,
    enabled: z.boolean(),
    disabledReason: z.string().trim().min(1).max(64).optional(),
  })
  .strict();

export const taskSummarySchema = z
  .object({
    id: idSchema,
    threadId: idSchema,
    turnId: idSchema.optional(),
    projectId: idSchema,
    title: z.string().trim().min(1).max(28),
    status: taskStatusSchema,
    summary: z.string().trim().min(1).max(64),
    detail: z.string().trim().min(1).max(768).optional(),
    startedAt: isoTimestampSchema,
    updatedAt: isoTimestampSchema,
    elapsedSeconds: z.number().int().nonnegative(),
    requiresAttention: z.boolean(),
    pendingApprovalId: idSchema.optional(),
    macros: z.array(macroDescriptorSchema).max(12),
  })
  .strict();

export const approvalDecisionSchema = z.enum(["accept", "decline", "cancel"]);

export const approvalRequestSchema = z
  .object({
    id: idSchema,
    taskId: idSchema,
    type: z.enum(["command", "file_change", "network", "permissions"]),
    title: z.string().trim().min(1).max(32),
    summary: z.string().trim().min(1).max(160),
    command: z.string().max(512).optional(),
    cwd: z.string().max(160).optional(),
    risk: z.enum(["low", "medium", "high"]),
    riskReason: z.string().trim().min(1).max(96).optional(),
    createdAt: isoTimestampSchema,
    expiresAt: isoTimestampSchema.optional(),
    allowedDecisions: z.array(approvalDecisionSchema).min(1).max(3),
  })
  .strict();

const requestIdSchema = idSchema;
const taskIdSchema = idSchema;

export const helloMessageSchema = z
  .object({
    type: z.literal("hello"),
    protocol: z.literal(PROTOCOL_VERSION),
    deviceId: idSchema,
    deviceName: z.string().trim().min(1).max(32),
    firmwareVersion: z.string().trim().min(1).max(24),
    capabilities: z
      .array(z.enum(["keyboard", "display", "hold-confirm"]))
      .max(8),
  })
  .strict();

export const deviceMessageSchema = z.discriminatedUnion("type", [
  helloMessageSchema,
  z.object({ type: z.literal("pong"), nonce: idSchema }).strict(),
  z.object({ type: z.literal("task.select"), taskId: taskIdSchema }).strict(),
  z
    .object({
      type: z.literal("task.stop.request"),
      requestId: requestIdSchema,
      taskId: taskIdSchema,
    })
    .strict(),
  z
    .object({
      type: z.literal("task.followup.submit"),
      requestId: requestIdSchema,
      taskId: taskIdSchema,
      prompt: z
        .string()
        .trim()
        .min(1)
        .refine(
          (value) => Buffer.byteLength(value, "utf8") <= MAX_FOLLOWUP_BYTES,
          {
            message: `Follow-up exceeds ${MAX_FOLLOWUP_BYTES} UTF-8 bytes`,
          },
        ),
    })
    .strict(),
  z
    .object({
      type: z.literal("workflow.launch.request"),
      requestId: requestIdSchema,
      projectId: idSchema,
      workflowId: idSchema,
    })
    .strict(),
  z
    .object({
      type: z.literal("skill.launch.request"),
      requestId: requestIdSchema,
      projectId: idSchema,
      skillName: z.string().regex(/^[A-Za-z0-9][A-Za-z0-9:_-]{0,127}$/),
      prompt: z.string().trim().max(8000).optional(),
    })
    .strict(),
  z
    .object({
      type: z.literal("approval.respond"),
      requestId: requestIdSchema,
      approvalId: idSchema,
      decision: approvalDecisionSchema,
    })
    .strict(),
  z
    .object({
      type: z.literal("tasks.clear.request"),
      requestId: requestIdSchema,
    })
    .strict(),
  z.object({ type: z.literal("snapshot.request") }).strict(),
]);

export const serverMessageSchema = z.discriminatedUnion("type", [
  z
    .object({
      type: z.literal("welcome"),
      protocol: z.literal(PROTOCOL_VERSION),
      bridgeVersion: z.string().trim().min(1).max(24),
      serverTime: isoTimestampSchema,
      heartbeatSeconds: z.number().int().positive().max(60),
    })
    .strict(),
  z
    .object({
      type: z.literal("task.snapshot"),
      tasks: z.array(taskSummarySchema).max(MAX_SNAPSHOT_TASKS),
    })
    .strict(),
  z
    .object({ type: z.literal("task.upsert"), task: taskSummarySchema })
    .strict(),
  z.object({ type: z.literal("task.remove"), taskId: taskIdSchema }).strict(),
  z
    .object({
      type: z.literal("approval.open"),
      approval: approvalRequestSchema,
    })
    .strict(),
  z
    .object({
      type: z.literal("approval.resolved"),
      approvalId: idSchema,
      decision: approvalDecisionSchema.optional(),
    })
    .strict(),
  z
    .object({
      type: z.literal("macro.snapshot"),
      macros: z.array(macroDescriptorSchema).max(20),
    })
    .strict(),
  z
    .object({
      type: z.literal("usage.update"),
      available: z.boolean(),
      limitName: z.string().trim().min(1).max(32).optional(),
      primaryRemainingPercent: z.number().int().min(0).max(100).optional(),
      primaryResetsAt: z.number().int().nonnegative().optional(),
      primaryWindowMinutes: z.number().int().positive().optional(),
      secondaryRemainingPercent: z.number().int().min(0).max(100).optional(),
      secondaryResetsAt: z.number().int().nonnegative().optional(),
      secondaryWindowMinutes: z.number().int().positive().optional(),
    })
    .strict(),
  z
    .object({
      type: z.literal("toast"),
      level: z.enum(["info", "success", "warning", "error"]),
      message: z.string().trim().min(1).max(96),
      requestId: requestIdSchema.optional(),
    })
    .strict(),
  z
    .object({
      type: z.literal("error"),
      requestId: requestIdSchema.optional(),
      code: z.string().regex(/^[A-Z][A-Z0-9_]{1,63}$/),
      message: z.string().trim().min(1).max(160),
      recoverable: z.boolean(),
    })
    .strict(),
  z.object({ type: z.literal("ping"), nonce: idSchema }).strict(),
]);

export const protocolBundleSchema = z
  .object({
    deviceMessage: deviceMessageSchema,
    serverMessage: serverMessageSchema,
  })
  .strict();
