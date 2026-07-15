import type { MacroDescriptor } from "@codexdeck/protocol";

import type { SkillMetadata } from "../codex/types.js";
import type { WorkflowRegistry } from "./workflow-registry.js";

export interface SkillLister {
  listSkills(cwd: string): Promise<SkillMetadata[]>;
}

export async function discoverSkillMacros(
  appServer: SkillLister,
  workflows: WorkflowRegistry,
  onError?: (projectId: string, error: unknown) => void,
): Promise<MacroDescriptor[]> {
  const macros: MacroDescriptor[] = [];
  for (const project of workflows.list()) {
    try {
      const skills = await appServer.listSkills(project.cwd);
      for (const skill of skills.sort((left, right) =>
        left.name.localeCompare(right.name),
      )) {
        macros.push({
          id: `skill:${project.id}/${skill.name}`,
          label: `${project.label}: ${skill.name}`.slice(0, 48),
          shortLabel: skill.name.slice(0, 16),
          action: "launch_skill",
          confirmation: "none",
          enabled: true,
        });
      }
    } catch (error) {
      onError?.(project.id, error);
    }
  }
  return macros.slice(0, 20);
}
