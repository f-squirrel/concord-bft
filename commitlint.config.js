//module.exports = {
//
//    extends: ['@commitlint/config-conventional'],
//    rules: {
//        'footer-leading-blank': [2, 'always'],
//        'footer-max-line-length': [2, 'always', 120],
//        'body-max-line-length': [2, 'always', 120],
//    },
//}
module.exports = {



    extends: ['@commitlint/config-conventional'],
	rules: {
		'body-leading-blank': [2, 'always'],
		'body-max-line-length': [2, 'always', 120],
		'header-max-length': [2, 'always', 69],
		'subject-case': [
			2,
			'always',
			['lower-case', 'camel-case', 'kebab-case', 'snake-case', 'pascal-case', 'upper-case'],
		],
		'subject-empty': [0, 'never'],
		'subject-full-stop': [2, 'never', '.'],
        'footer-leading-blank': [0, 'always'],
		'footer-max-line-length': [0, 'always', 100],
		'scope-case': [0, 'always', 'lower-case'],
		'type-case': [0, 'always', 'lower-case'],
		'type-empty': [0, 'never'],
		'type-enum': [
			0,
			'always',
			[],
		],
	},
};;
