import { Component, computed, input } from '@angular/core';
import { TableView } from '../features/table/table-view';
import { SCENARIO_JOURNAL, ScenarioName, SCENARIOS, scenarioView } from './fixtures';

/** Page `/dev/table?scenario=uno-window&theme=nuit` : la table alimentée par une situation écrite à la main. */
@Component({
  selector: 'app-table-fixture',
  imports: [TableView],
  template: `<app-table-view [view]="view()" [journal]="journal" />`,
})
export class TableFixture {
  readonly scenario = input<string>('uno-window');

  protected readonly journal = SCENARIO_JOURNAL;
  protected readonly view = computed(() => {
    const requested = this.scenario();
    const name = SCENARIOS.find((s) => s === requested) ?? ('uno-window' satisfies ScenarioName);
    return scenarioView(name, Date.now());
  });
}
