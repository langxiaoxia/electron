const fs = require('fs');
const path = require('path');
const semver = require('semver');

//+by xxlang@2021-12-15 {
const moment = require('moment');
moment.locale('zh-cn');
const tag = moment().format('YYYYMMDD');
//+by xxlang@2021-12-15 }

const outputPath = process.argv[2];

const currentVersion = fs.readFileSync(path.resolve(__dirname, '../ELECTRON_VERSION'), 'utf8').trim();

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
  tag, //+by xxlang@2021-12-09
  major,
  minor,
  patch,
  prerelease,
  has_prerelease: prerelease === '' ? 0 : 1
}, null, 2));
