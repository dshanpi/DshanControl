---
name: qt-industrial-ui
description: Design and review DshanControl Qt5 desktop applications, subpages and dialogs. Use for navigation, industrial controls, media, settings, cloud/home workflows, status feedback or visual changes. Prioritize visible actions and truthful end-to-end flows at 1024x600.
---

# Qt desktop interaction design

Read the repository AGENTS.md and docs/design/qt-application-interaction.md before UI work. The IBM reference in awesome-design-md provides a restrained visual direction; it is a marketing-site analysis, not an official Carbon or Qt implementation specification.

## Work from tasks
1. Inventory every affected application, subpage, dialog and return path. Describe the user's task, prerequisites, visible primary action, pending state, confirmed result, failure recovery and cancel behavior.
2. Read actual Qt widgets, routing and backend contracts. Reuse capability, confirmation and lifecycle checks. A repaint is not a workflow repair.
3. Make actions discoverable without hover: Chinese text on primary buttons, file navigation, transport and confirmation controls. Icons supplement labels. Show selected tabs and location. Keep access to all applications and the workbench.
4. Separate observed service state from the last operation result. Polling must not erase feedback. Do not infer success from a click, request acceptance, mock data or a rendered screen.
5. Implement the actual application. Use standard Qt5 widgets with a shared style and translated, touch-sized dialogs; avoid heavyweight browser layers, dependencies or decorative effects.
6. Exercise normal, unavailable, empty, pending, failed and canceled flows using isolated host fixtures. Include file/folder navigation, keyboard focus, touch-sized targets, back navigation and state retention. Never invoke real hardware through screenshot/test code.
7. Render actual application routes and nested tabs at 1024x600. Inspect images for clipped controls, missing CJK glyphs, ambiguous status and inaccessible actions. Preserve approved reference images; save new reviews separately.
8. Run required repository checks. Record fixture limitations and remaining real-device evidence. Source sync does not authorize hardware deployment.

## Visual rules
Use the accompanying design contract: neutral surfaces, blue primary action/focus, explicit hierarchy, 4px spacing increments, visible labels and 48px main controls. Preserve the bundled Chinese font. Do not copy marketing typography, logo, oversized hero sections or claim Carbon certification.

## Stop conditions
Do not disguise missing integrations as working controls. Keep unavailable reasons and recovery steps visible. No synthetic touch, media playback or host screenshots can establish physical touch, decoder, audio or fieldbus acceptance.

## Required design-first page gate
Follow docs/design/page-delivery-gate.md. Render before implementation, show the user, resolve recorded gaps and preserve actual review references. Keep an individual page ledger for business flow, operation steps, function changes, failure recovery, source hashes and verification. Run the design-phase gate before implementation and make check-ui-pages before UI verification/package. Generated concepts do not qualify as actual Qt screenshots. Never manufacture approval or hardware evidence.
