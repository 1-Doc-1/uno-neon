import { DEV_ROUTES as developmentRoutes } from './dev.routes';
import { DEV_ROUTES as productionRoutes } from './dev.routes.prod';

describe('dev routes', () => {
  it('exist in development', () => {
    expect(developmentRoutes.map((route) => route.path)).toEqual(['dev', 'dev/table']);
  });

  it('are replaced by nothing in the production build', () => {
    expect(productionRoutes).toEqual([]);
  });
});
