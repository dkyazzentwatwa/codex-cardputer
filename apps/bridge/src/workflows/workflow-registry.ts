import { access, readFile } from "node:fs/promises";
import path from "node:path";

import { parseDocument } from "yaml";
import { z } from "zod";

const IDENTIFIER = /^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$/;
const SKILL_IDENTIFIER = /^[A-Za-z0-9][A-Za-z0-9:_-]{0,127}$/;

const workflowSchema = z
  .object({
    label: z.string().trim().min(1).max(48),
    shortLabel: z.string().trim().min(1).max(16),
    prompt: z.string().trim().min(1).max(8000),
    skill: z.string().regex(SKILL_IDENTIFIER).optional(),
  })
  .strict();

const projectSchema = z
  .object({
    label: z.string().trim().min(1).max(48),
    cwd: z.string().min(1),
    model: z.string().trim().min(1).max(80).optional(),
    approvalPolicy: z
      .enum(["untrusted", "on-request", "never"])
      .default("on-request"),
    sandbox: z
      .enum(["read-only", "workspace-write", "danger-full-access"])
      .default("workspace-write"),
    workflows: z.record(z.string().regex(IDENTIFIER), workflowSchema),
  })
  .strict();

const registrySchema = z
  .object({ projects: z.record(z.string().regex(IDENTIFIER), projectSchema) })
  .strict();

export type WorkflowDefinition = z.infer<typeof workflowSchema>;
export type ProjectDefinition = z.infer<typeof projectSchema> & { id: string };

export class WorkflowRegistry {
  private projects = new Map<string, ProjectDefinition>();

  async load(filePath: string): Promise<void> {
    const document = parseDocument(await readFile(filePath, "utf8"), {
      uniqueKeys: true,
    });
    if (document.errors.length > 0)
      throw new Error(document.errors.map((error) => error.message).join("; "));
    const parsed = registrySchema.parse(document.toJS());
    const next = new Map<string, ProjectDefinition>();
    for (const [id, project] of Object.entries(parsed.projects)) {
      if (!path.isAbsolute(project.cwd))
        throw new Error(`Project ${id} cwd must be absolute`);
      await access(project.cwd);
      next.set(id, { id, ...project });
    }
    this.projects = next;
  }

  getProject(projectId: string): ProjectDefinition {
    const project = this.projects.get(projectId);
    if (!project) throw new Error(`Unknown project: ${projectId}`);
    return project;
  }

  getWorkflow(projectId: string, workflowId: string): WorkflowDefinition {
    const workflow = this.getProject(projectId).workflows[workflowId];
    if (!workflow)
      throw new Error(`Unknown workflow: ${projectId}/${workflowId}`);
    return workflow;
  }

  list(): ProjectDefinition[] {
    return [...this.projects.values()];
  }
}
