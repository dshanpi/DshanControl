# DshanControl Showcase

Five-page public industrial gateway demonstration implemented from the approved
1672×941 visual references. The website uses Vue 3, TypeScript, Three.js,
Vue Flow and the existing Flask service.

```sh
npm ci
npm run dev
npm test
npm run build
npm run test:e2e
```

The production build is written to `components/web/assets`. Flask serves the
public application at `/` and `/experience/*`; the original authenticated
management interface remains at `/manage`.

GitHub Actions reads the repository's Pages `base_path` and passes it to Vite.
Vue Router uses the same value and the deployment creates a `404.html` SPA
fallback, so both the `control.100ask.net` custom domain and direct links under
`/experience/*` keep working.

The interactive industrial equipment is a set of editable procedural concept
models in `IndustrialScene.vue`. It is true WebGL geometry with separate
objects and picking, but it is not product CAD. If WebGL is unavailable the
same device relationships remain usable in an explicitly labelled compatibility
view.

The default data source is deterministic demonstration data. `LiveProvider`
adapts the existing authenticated APIs but this release does not claim real
peripheral read/write acceptance.
