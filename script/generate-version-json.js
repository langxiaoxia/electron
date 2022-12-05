const fs = require('fs');
const path = require('path');
const semver = require('semver');

// +by xxlang@2021-12-15 {
const moment = require('moment');
moment.locale('zh-cn');
const buildDate = moment().format('YYYY-MM-DD');
const git = require('git-rev-sync');
const buildRev = git.short();
const tag = buildRev + '@' + buildDate;
// +by xxlang@2021-12-15 }

const outputPath = process.argv[2];
const currentVersion = process.argv[3];

const parsed = semver.parse(currentVersion);

let prerelease = '';
if (parsed.prerelease && parsed.prerelease.length > 0) {
  prerelease = parsed.prerelease.join('.');
}

const {
  major,
  minor,
  patch
} = parsed;

fs.writeFileSync(outputPath, JSON.stringify({
  tag, // +by xxlang@2021-12-09
  full_version: currentVersion,
  major,
  minor,
  patch,
  prerelease,
  prerelease_number: prerelease ? parsed.prerelease[parsed.prerelease.length - 1] : '0',
  has_prerelease: prerelease === '' ? 0 : 1
}, null, 2));
