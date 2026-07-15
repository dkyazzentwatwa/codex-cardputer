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
  const projectMacros: MacroDescriptor[][] = [];
  for (const project of workflows.list()) {
    try {
      const skills = await appServer.listSkills(project.cwd);
      projectMacros.push(
        skills
          .sort((left, right) => left.name.localeCompare(right.name))
          .map((skill) => ({
            id: `skill:${project.id}/${skill.name}`,
            label: `${project.label}: ${skill.name}`.slice(0, 48),
            shortLabel: skill.name.slice(0, 16),
            action: "launch_skill",
            confirmation: "none",
            enabled: true,
          })),
      );
    } catch (error) {
      projectMacros.push([]);
      onError?.(project.id, error);
    }
  }

  const macros: MacroDescriptor[] = [];
  for (let index = 0; macros.length < 20; ++index) {
    let added = false;
    for (const project of projectMacros) {
      const macro = project[index];
      if (!macro) continue;
      macros.push(macro);
      added = true;
      if (macros.length === 20) break;
    }
    if (!added) break;
  }
  return macros.slice(0, 20);
}
