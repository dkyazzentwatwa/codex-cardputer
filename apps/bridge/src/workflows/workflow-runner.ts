import { randomUUID } from "node:crypto";

import type { AppServerThread, AppServerTurn, AppServerUserInput, SkillMetadata } from "../codex/types.js";
import { truncateAtWord } from "../tasks/summary-builder.js";
import type { ManagedTask } from "../tasks/task-registry.js";
import type { TaskRegistry } from "../tasks/task-registry.js";
import type { WorkflowRegistry } from "./workflow-registry.js";

export interface WorkflowAppServer {
  startThread(params: {
    cwd: string;
    model?: string;
    approvalPolicy?: "untrusted" | "on-request" | "never";
    sandbox?: "read-only" | "workspace-write" | "danger-full-access";
  }): Promise<AppServerThread>;
  startTurn(threadId: string, input: AppServerUserInput[]): Promise<AppServerTurn>;
  steerTurn(threadId: string, expectedTurnId: string, input: AppServerUserInput[]): Promise<void>;
  interruptTurn(threadId: string, turnId: string): Promise<void>;
  listSkills(cwd: string): Promise<SkillMetadata[]>;
}

function textInput(text: string): AppServerUserInput {
  return { type: "text", text, text_elements: [] };
}

export class WorkflowRunner {
  constructor(
    private readonly appServer: WorkflowAppServer,
    private readonly workflows: WorkflowRegistry,
    private readonly tasks: TaskRegistry,
  ) {}

  async launchWorkflow(projectId: string, workflowId: string): Promise<ManagedTask> {
    const project = this.workflows.getProject(projectId);
    const workflow = this.workflows.getWorkflow(projectId, workflowId);
    const thread = await this.appServer.startThread({
      cwd: project.cwd,
      ...(project.model ? { model: project.model } : {}),
      approvalPolicy: project.approvalPolicy,
      sandbox: project.sandbox,
    });
    const now = new Date().toISOString();
    const task = this.tasks.create({
      id: randomUUID(),
      threadId: thread.id,
      projectId,
      workflowId,
      title: truncateAtWord(`${project.label}: ${workflow.shortLabel}`, 28),
      status: "starting",
      summary: "Starting Codex workflow",
      startedAt: now,
      updatedAt: now,
    });
    try {
      const input = await this.buildInput(project.cwd, workflow.prompt, workflow.skill);
      const turn = await this.appServer.startTurn(thread.id, input);
      return this.tasks.beginTurn(task.id, turn.id);
    } catch (error) {
      this.tasks.transition(task.id, "failed", error instanceof Error ? error.message : "Workflow failed");
      throw error;
    }
  }

  async launchSkill(projectId: string, skillName: string, prompt?: string): Promise<ManagedTask> {
    const project = this.workflows.getProject(projectId);
    const thread = await this.appServer.startThread({
      cwd: project.cwd,
      ...(project.model ? { model: project.model } : {}),
      approvalPolicy: project.approvalPolicy,
      sandbox: project.sandbox,
    });
    const now = new Date().toISOString();
    const task = this.tasks.create({
      id: randomUUID(),
      threadId: thread.id,
      projectId,
      title: truncateAtWord(`${project.label}: ${skillName}`, 28),
      status: "starting",
      summary: "Starting Codex skill",
      startedAt: now,
      updatedAt: now,
    });
    try {
      const input = await this.buildInput(project.cwd, prompt ?? `Use ${skillName} for this project.`, skillName);
      const turn = await this.appServer.startTurn(thread.id, input);
      return this.tasks.beginTurn(task.id, turn.id);
    } catch (error) {
      this.tasks.transition(task.id, "failed", error instanceof Error ? error.message : "Skill failed");
      throw error;
    }
  }

  async followup(taskId: string, prompt: string): Promise<ManagedTask> {
    const task = this.tasks.require(taskId);
    const input = [textInput(prompt)];
    if (["starting", "running", "waiting_approval", "waiting_input"].includes(task.status) && task.turnId) {
      await this.appServer.steerTurn(task.threadId, task.turnId, input);
      return this.tasks.updateSummary(task.id, "Follow-up sent");
    }
    const turn = await this.appServer.startTurn(task.threadId, input);
    return this.tasks.beginTurn(task.id, turn.id);
  }

  async stop(taskId: string): Promise<ManagedTask> {
    const task = this.tasks.require(taskId);
    if (!task.turnId || !["starting", "running", "waiting_approval", "waiting_input"].includes(task.status)) {
      throw new Error("Task is not running");
    }
    await this.appServer.interruptTurn(task.threadId, task.turnId);
    return this.tasks.transition(task.id, "cancelled", "Task cancelled");
  }

  private async buildInput(cwd: string, prompt: string, skillName?: string): Promise<AppServerUserInput[]> {
    if (!skillName) return [textInput(prompt)];
    const skill = (await this.appServer.listSkills(cwd)).find((candidate) => candidate.name === skillName);
    if (!skill) throw new Error(`Skill is not available for project: ${skillName}`);
    return [{ type: "skill", name: skill.name, path: skill.path }, textInput(prompt)];
  }
}
